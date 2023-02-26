
#include "network.h"

Network::Network(int NUMBER_OF_DEVICES) {
    
    Network::NUMBER_OF_DEVICES = NUMBER_OF_DEVICES;
    
    // initialize sockets
    int response;
    
    ssdp_sckt = create_socket(SSDP_PORT);
    ack_sckt = create_socket(ACK_PORT);
    chlg_sckt = create_socket(CHLG_PORT);
    ntp_sckt = create_socket(NTP_PORT);
    
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

int Network::create_socket(int port) {
    
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
    timeout.tv_sec = 3;
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
                if (std::strcmp(ifaddr_iter->ifa_name, "en1") == 0) {
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

void Network::send_ssdp_message(int sckt, const char* msg) {
    
    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SSDP_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SSDP_ADDR);
    
    if (sendto(sckt, (const void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while sending message to " << inet_ntoa(dest_addr.sin_addr) << ": " << std::strerror(errno) << std::endl;
    }
}

void Network::send_ack_message(int sckt, in_addr_t addr, const char* msg) {
    
    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(ACK_PORT);
    dest_addr.sin_addr.s_addr = addr;
    
    if (sendto(sckt, (const void*)(intptr_t) msg, sizeof(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while acknowledging message: " << strerror(errno) << std::endl;
    }
}

void Network::receive_ssdp_message(int sckt, std::vector<char*>& devices, bool& discovering) {
    
    while (discovering) {
        
        char buffer[1024];
        std::memset(&buffer, 0, sizeof(buffer));
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        ssize_t response = recvfrom(sckt, &buffer, sizeof(buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len);
        
        if (response < 0) {
            if (errno != 0x23) {
                std::cerr << "Error while receiving ACK: " << std::strerror(errno) << std::endl;
            }
            continue;
        }
        
        char* device = new char[INET_ADDRSTRLEN];
        std::memset(device, 0, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);
        
        if (std::strcmp(device, net_config.address) == 0) {
            continue;
        }
        
        bool has_device = false;
        
        for (auto x : devices) {
            if (std::strcmp(x, device) == 0) {
                has_device = true;
                break;
            }
        }
        
        if (!has_device) {
            devices.push_back(device);
            std::cout << "Discovered device: " << device << std::endl;
        } else {
            delete[] device;
        }
    }
}

void Network::receive_ack_message(int sckt, std::vector<char*>& pending_devices, std::vector<char*>& devices, bool& acknowledging) {
    
    while (acknowledging) {
        
        char buffer[1024];
        std::memset(&buffer, 0, sizeof(buffer));
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        ssize_t response = recvfrom(sckt, &buffer, sizeof(buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len);
        
        if (response < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error while receiving ACK: " << std::strerror(errno) << std::endl;
            }
            continue;
        }
        
        char* device = new char[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);
        
        if (std::string(device) == std::string(net_config.address)) {
            delete[] device;
            continue;
        }
        
        if (std::strncmp(buffer, "ACK", std::max(strlen(buffer), strlen("ACK"))) == 0) {
            if (std::find_if(pending_devices.begin(), pending_devices.end(), [device](const char* c) { return std::string(c) == std::string(device); }) == pending_devices.end()) {
                pending_devices.push_back(device);
                send_ack_message(sckt, src_addr.sin_addr.s_addr, "ACKACK");
            }
            continue;
        } else if (std::strcmp(buffer, "ACKACK") == 0) {
            if (std::find_if(devices.begin(), devices.end(), [device](const char* c) { return std::string(c) == std::string(device); }) == devices.end()) {
                std::cout << "Acknowledged Device: " << device << std::endl;
                devices.push_back(device);
            }
            continue;
        }
        
        delete[] device;
    }
}

void Network::send_message(int sckt, char* addr, const char* msg, int port) {
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_port = htons(port);
    
    if (sendto(sckt, (void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while sending message: " << std::strerror(errno) << std::endl;
    }
}

void Network::receive_message(int sckt, char**& msg_buffer, char**& addr_buffer, bool* receiving) {
    
    while (*receiving) {
        
        char buffer[8];
        memset(&buffer, 0, sizeof(buffer));
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(sckt, &buffer, sizeof(buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error while receiving message: " << std::strerror(errno) << std::endl;
            }
            continue;
        }

        char* device = new char[INET_ADDRSTRLEN];
        std::memset(device, 0, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);
        
        for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
            if (msg_buffer[i] == nullptr) {
                // allocate memory for message
                msg_buffer[i] = new char[src_addr_len];
                std::memcpy(msg_buffer[i], &buffer, src_addr_len);
                // point to device memory
                addr_buffer[i] = device;
                return;
            }
        }
    }
}

void Network::discover_devices() {
    
    std::vector<char*> discovered_devices;
    
    std::cout << "Discovering Devices..." << std::endl;
    
    bool discovering = true;
    std::thread receive_thread_ssdp(&Network::receive_ssdp_message, this, ssdp_sckt, std::ref(discovered_devices), std::ref(discovering));
    
    const char* message =   "M-SEARCH * HTTP/1.1\r\n"
                            "HOST: 239.255.255.250:1900\r\n"
                            "MAN: \"ssdp:notify\"\r\n"
                            "ST: ssdp:all\r\n"
                            "MX: 3\r\n"
                            "\r\n";
    
    // discovery phase
    for (int _=0; _!=10; _++) {
        send_ssdp_message(ssdp_sckt, message);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    discovering = false;
    
    receive_thread_ssdp.join();
    close(ssdp_sckt);
    
    std::cout << "Discovered " << discovered_devices.size() << " devices." << std::endl;
    std::cout << "Acknowledging Devices..." << std::endl;
    
    std::vector<char*> pending_acknowledgements;
    std::vector<char*> acknowledged_devices;
    
    // acknowledgement phase
    bool acknowledging = true;
    
    std::thread receive_thread_ack(&Network::receive_ack_message, this, ack_sckt, std::ref(pending_acknowledgements), std::ref(acknowledged_devices), std::ref(acknowledging));
    
    while (acknowledged_devices.size() != NUMBER_OF_DEVICES-1) {
        for (auto x : discovered_devices) {
            if (std::find_if(acknowledged_devices.begin(), acknowledged_devices.end(), [x](const char* c) { return std::string(x) == std::string(c); }) == acknowledged_devices.end()) {
                std::this_thread::sleep_for(std::chrono::milliseconds((std::rand() % 1000) + 100));
                send_ack_message(ack_sckt, inet_addr(x), "ACK");
            }
        }
    }
    
    acknowledging = false;
    
    receive_thread_ack.join();
    
    close(ack_sckt);
    
    net_config.devices = new char*[acknowledged_devices.size()];
    
    std::cout << "Found " << acknowledged_devices.size() << " devices:" << std::endl;
    for (int i=0; i!=acknowledged_devices.size(); i++) {
        auto x = acknowledged_devices[i];
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
