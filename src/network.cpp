
#include "network.h"

Network::Network(int NUMBER_OF_DEVICES) {
    
    Network::NUMBER_OF_DEVICES = NUMBER_OF_DEVICES;
    
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
    timeout.tv_sec = 1;
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

bool Network::listen_for_ack(char *addr) {
    
    char* recv_buffer[8];
    std::memset(recv_buffer, 0, sizeof(recv_buffer));
    
    sockaddr_in src_addr;
    std::memset(&src_addr, 0, sizeof(src_addr));
    socklen_t src_addr_len = sizeof(src_addr);
    
    if (recvfrom(ack_sckt, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
        return false;
    }
    
    if (std::strcmp((const char*) recv_buffer, "ACK") == 0) {
        return true;
    }
    
    return false;
}

void Network::send_message(int sckt, char* addr, int port, const char* msg, short timeout=3) {
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_port = htons(port);
    
    if (sendto(sckt, (void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while sending message: " << std::strerror(errno) << std::endl;
    }
    
    if (!listen_for_ack(addr)) {
        
        if (timeout == 0) {
            return;
        }
        
        send_message(sckt, addr, port, msg, timeout-1);
    }
}

void Network::receive_messages(int sckt, bool& receiving) {
    
    while (receiving) {
        
        char recv_buffer[128];
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
            send_message(sckt, device, ACK_PORT, "ACK");
        }
        
        char* buffer_msg = new char[src_addr_len + 2 + std::strlen(recv_buffer)];
        std::strcat(buffer_msg, device);
        std::strcat(buffer_msg, "::");
        std::strcat(buffer_msg, recv_buffer);
        
        if (current_index < BUFFER_SIZE) {
            RECEIVING_BUFFER[current_index] = new char[std::strlen(buffer_msg)];
            std::memcpy(RECEIVING_BUFFER[current_index], buffer_msg, std::strlen(buffer_msg));
            current_index++;
        } else {
            RECEIVING_BUFFER++;
            RECEIVING_BUFFER[current_index-1] = new char[std::strlen(buffer_msg)];
            std::memcpy(RECEIVING_BUFFER[current_index-1], buffer_msg, std::strlen(buffer_msg));
        }
    }
}

void Network::discover_devices() {
    
    std::vector<char*> discovered_devices;
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
        send_message(ssdp_sckt, (char*) SSDP_ADDR, SSDP_PORT, message);
        for (int i=0; i!=current_index; i++) {
            if (RECEIVING_BUFFER[i]) {
                
                std::string buffer = std::string(RECEIVING_BUFFER[i]);
                int delimiter_index = static_cast<int>(buffer.find_last_of(":"));
                
                std::string addr = buffer.substr(0, delimiter_index);
                std::string msg = buffer.substr(delimiter_index+1, buffer.size());
                
                if (msg == "ACK" && std::find_if(discovered_devices.begin(), discovered_devices.end(), [local_addr, addr](char* c) {
                    return std::string(local_addr) != std::string(addr) && std::string(addr) == std::string(c);
                }) == discovered_devices.end()) {
                    discovered_devices.push_back((char*) addr.c_str());
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    discovering = false;
    
    receive_thread_ssdp.join();
    close(ssdp_sckt);
    close(ack_sckt);
    
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
