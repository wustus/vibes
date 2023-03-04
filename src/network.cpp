
#include "network.h"

Network::Network(int NUMBER_OF_DEVICES) {
    
    Network::NUMBER_OF_DEVICES = NUMBER_OF_DEVICES;
    
    RECEIVING_BUFFER = new char*[BUFFER_SIZE];
    for (int i=0; i!=BUFFER_SIZE; i++) {
        RECEIVING_BUFFER[i] = new char[MESSAGE_SIZE];
        RECEIVING_BUFFER[i][0] = '\0';
    }
    
    // initialize sockets
    int response;
    
    ssdp_sckt = create_udp_socket(SSDP_PORT);
    ack_sckt = create_udp_socket(ACK_PORT);
    chlg_sckt = create_udp_socket(CHLG_PORT);
    ntp_sckt = create_udp_socket(NTP_PORT);
    
    // join ssdp multicast group
    struct ip_mreq mreq;
    std::memset(&mreq, 0, sizeof(mreq));
    
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    response = setsockopt(ssdp_sckt, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    if (response < 0) {
        std::cerr << "Error setting Multicast socket option: " << std::strerror(errno) << std::endl;
        exit(1);
    }
}

Network::~Network() {
    close(ssdp_sckt);
    close(ack_sckt);
    close(chlg_sckt);
    close(ntp_sckt);
    
    for (int i=0; i!=BUFFER_SIZE; i++) {
        delete[] RECEIVING_BUFFER[i];
    }
    
    delete[] RECEIVING_BUFFER;
}

int Network::create_udp_socket(int port) {
    
    int sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    int opt = 1;
    
    // reuse address
    if (setsockopt(sckt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error while setting socket option for port" << port << ": " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    
    // receive timeout
    struct timeval timeout;
    std::memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    
    if (setsockopt(sckt, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error while setting timeout for port " << port << ": " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    // bind port
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sckt, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) < 0) {
        std::cerr << "Error while binding address to port " << port << ": " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    return sckt;
}

void Network::set_local_addr() {
    
    struct ifaddrs *ifaddr, *ifaddr_iter;
    void* addr;
    char ip_addr[INET_ADDRSTRLEN];
    
    if (getifaddrs(&ifaddr) < 0) {
        std::cerr << "Error while getting network interface addresses: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    for (ifaddr_iter = ifaddr; ifaddr_iter != nullptr; ifaddr_iter = ifaddr_iter->ifa_next) {
        if (ifaddr_iter->ifa_addr == nullptr) {
            continue;
        }
        
        if (ifaddr_iter->ifa_addr->sa_family == AF_INET) {
            addr = &((struct sockaddr_in*) ifaddr_iter->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ip_addr, INET_ADDRSTRLEN);
            #ifdef __APPLE__
                if (std::strcmp(ifaddr_iter->ifa_name, "en1") == 0 || std::strcmp(ifaddr_iter->ifa_name, "en0") == 0) {
                    memcpy((void*) net_config.address, ip_addr, sizeof(ip_addr));
                    return;
                }
            #endif
            
            #ifdef __unix
                if (std::strcmp(ifaddr_iter->ifa_name, "wlan0") == 0) {
                    memcpy((void*) net_config.address, ip_addr, sizeof(ip_addr));
                    return;
                }
            #endif
        }
    }
}

void Network::append_to_buffer(char* addr, char* message) {
    
    if (std::strlen(addr) + 2 + std::strlen(message) > MESSAGE_SIZE) {
        char error_msg[std::strlen(addr) + std::strlen(message) + 128];
        std::snprintf(error_msg, sizeof(error_msg), "Message too long for Buffer: \n\t%s::%s (%zu + 2 + %zu) \ntoo long for MESSAGE_SIZE=%d.", addr, message, std::strlen(addr), std::strlen(message), MESSAGE_SIZE);
        throw std::length_error(error_msg);
    }
    
    char buffer_msg[MESSAGE_SIZE];
    std::snprintf(buffer_msg, MESSAGE_SIZE, "%s::%s", addr, message);
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    
    std::strncpy(RECEIVING_BUFFER[current_index], buffer_msg, MESSAGE_SIZE-1);
    current_index = (current_index + 1) % BUFFER_SIZE;
    
    
}

bool Network::listen_for_ack(const char* addr) {
    
    char* recv_buffer[8];
    std::memset(recv_buffer, 0, sizeof(recv_buffer));
    
    sockaddr_in src_addr;
    std::memset(&src_addr, 0, sizeof(src_addr));
    socklen_t src_addr_len = sizeof(src_addr);
    
    if (recvfrom(ack_sckt, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
        return false;
    }
    
    append_to_buffer((char*) addr, (char*) recv_buffer);
    
    if (std::strcmp((const char*) recv_buffer, "ACK") == 0) {
        return true;
    }
    
    return false;
}

void Network::send_message(int sckt, const char* addr, int port, const char* msg, short timeout=3) {
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_port = htons(port);
    
    if (sendto(sckt, (void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while sending message: " << std::strerror(errno) << std::endl;
    }

    if (std::strcmp(msg, "ACK") == 0) {
        return;
    }
    
    if (!listen_for_ack(addr)) {
        
        if (timeout == 0) {
            std::cout << "Timeout of device " << addr << std::endl;
            return;
        }
        
        send_message(sckt, addr, port, msg, timeout-1);
    }
}

void Network::receive_messages(int sckt, bool& receiving) {
    
    while (receiving) {
        
        char recv_buffer[256];
        memset(&recv_buffer, 0, sizeof(recv_buffer));
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(sckt, &recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error while receiving message: " << std::strerror(errno) << std::endl;
            }
            continue;
        }
        
        char device[INET_ADDRSTRLEN];
        std::memset(&device, 0, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);

        if (std::strcmp(recv_buffer, "ACK") != 0) {
            send_message(ack_sckt, device, ACK_PORT, "ACK");
        }
        
        append_to_buffer(device, (char*) recv_buffer);
    }
}

void Network::discover_devices() {
    
    std::vector<const char*> discovered_devices;
    std::vector<std::thread> sending_threads;
    char* local_addr = net_config.address;
    
    std::cout << "Discovering Devices..." << std::endl;
    
    bool discovering = true;
    std::thread receive_thread_ssdp(&Network::receive_messages, this, ssdp_sckt, std::ref(discovering));
    
    const char* message =   "M-SEARCH * HTTP/1.1\r\n"
                            "HOST: 239.255.255.250:1900\r\n"
                            "MAN: \"ssdp:notify\"\r\n"
                            "ST: ssdp:all\r\n"
                            "MX: 3\r\n"
                            "\r\n";
    
    // discovery phase
    for (int _=0; _!=5; _++) {
        sending_threads.emplace_back([this, message]() { send_message(ssdp_sckt, SSDP_ADDR, SSDP_PORT, message); });
        for (int i=0; i!=current_index; i++) {
            if (RECEIVING_BUFFER[i]) {
                std::string buffer = std::string(RECEIVING_BUFFER[i]);
                int delimiter_index = static_cast<int>(buffer.find_first_of("::"));
                
                std::string addr = buffer.substr(0, delimiter_index);
                std::string msg = buffer.substr(delimiter_index+2, buffer.size());
                
                if (addr == std::string(SSDP_ADDR) || addr == std::string(local_addr) || addr == std::string(ROUTER_ADDR)) {
                    continue;
                }
                
                if (msg == "ACK" && std::find_if(discovered_devices.begin(), discovered_devices.end(), [addr](char* c) {
                    return std::string(addr) == std::string(c);
                }) == discovered_devices.end()) {
                    char* temp = new char[INET_ADDRSTRLEN];
                    std::memcpy(temp, RECEIVING_BUFFER[i], INET_ADDRSTRLEN);
                    discovered_devices.push_back(temp);
                    std::cout << "Address added: " << addr << std::endl;
                } else {
                    sending_threads.emplace_back([this, addr]() { send_message(ssdp_sckt, addr.c_str(), ACK_PORT, "BUDDY"); });
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    for (int i=0; i!=sending_threads.size(); i++) {
        sending_threads[i].join();
    }
    
    discovering = false;
    
    receive_thread_ssdp.join();
    close(ssdp_sckt);
    close(ack_sckt);
    
    std::cout << "BUFFER CONTENT" << std::endl;
    std::cout << "--------------" << std::endl;
    for (int i=0; i!=BUFFER_SIZE; i++) {
        if (*RECEIVING_BUFFER[i] != '\0') {
            std::cout << "Message " << i << std::endl;
            std::cout << RECEIVING_BUFFER[i] << std::endl << std::endl;
        }
    }
    
    std::cout << "Discovered " << discovered_devices.size() << " devices." << std::endl;
    
    net_config.devices = new char*[discovered_devices.size()];
    
    for (int i=0; i!=discovered_devices.size(); i++) {
        auto x = discovered_devices[i];
        net_config.devices[i] = new char[INET_ADDRSTRLEN];
        memcpy(net_config.devices[i], x, INET_ADDRSTRLEN);
        std::cout << "\t" << net_config.devices[i] << std::endl;
        
    }
}

int Network::get_ssdp_port() {
    return SSDP_PORT;
}

int Network::get_ssdp_sckt() {
    return ssdp_sckt;
}

int Network::get_ack_port() {
    return ACK_PORT;
}

int Network::get_ack_sckt() {
    return ack_sckt;
}

int Network::get_chlg_port() {
    return CHLG_PORT;
}

int Network::get_chlg_sckt() {
    return chlg_sckt;
}

int Network::get_ntp_port() {
    return NTP_PORT;
}

int Network::get_ntp_sckt() {
    return ntp_sckt;
}

int Network::get_number_of_devices() {
    return NUMBER_OF_DEVICES;
}

Network::NetworkConfig* Network::get_network_config() {
    return &net_config;
}
