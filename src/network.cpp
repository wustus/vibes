
#include "network.h"

Network::Network() {}

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
                    std::cout << &net_config.address << std::endl;
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
        
        std::cout << &net_config.address << std::endl;
        
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
    
    char* device = new char[INET_ADDRSTRLEN];
    struct sockaddr_in src_addr;
    
    while (receiving) {
        
        char* buffer = new char[1024];
        
        memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(sckt, &buffer, sizeof(buffer), 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error while receiving message: " << std::strerror(errno) << std::endl;
            }
            delete[] buffer;
            continue;
        }
        
        for (int i=0; i!=sizeof(net_config.devices) / sizeof(char*); i++) {
            if (msg_buffer[i] == nullptr) {
                memcpy(&msg_buffer[i], buffer, std::strlen(buffer));
                
                inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);
                addr_buffer[i] = device;
            }
        }
    }
}

void Network::discover_devices(int n_devices) {
    
    int response;
	
    int sckt_ssdp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int sckt_ack = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if (sckt_ssdp < 0) {
        std::cerr << "Error creating socket: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    int opt = 1;
    response = setsockopt(sckt_ssdp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (response < 0) {
        std::cerr << "Error setting reuse socket option: " << std::strerror(response) << std::endl;
        exit(1);
    }
    
    response = setsockopt(sckt_ack, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (response < 0) {
        std::cerr << "Error setting socket option: " << std::strerror(response) << std::endl;
        exit(1);
    }
    
    struct timeval timeout;
    std::memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    response = setsockopt(sckt_ssdp, SOL_SOCKET, SO_RCVTIMEO, &(timeout), sizeof(timeout));
    
    if (response < 0) {
        std::cerr << "Error setting timeout socket option: " << std::strerror(response) << std::endl;
        exit(1);
    }
    
    response = setsockopt(sckt_ack, SOL_SOCKET, SO_RCVTIMEO, &(timeout), sizeof(timeout));
    
    if (response < 0) {
        std::cerr << "Error setting timeout socket option: " << std::strerror(response) << std::endl;
        exit(1);
    }
    
    struct sockaddr_in bind_addr_ssdp, bind_addr_ack;
    std::memset(&bind_addr_ssdp, 0, sizeof(bind_addr_ssdp));
    std::memset(&bind_addr_ack, 0, sizeof(bind_addr_ack));
    
    bind_addr_ssdp.sin_family = AF_INET;
    bind_addr_ssdp.sin_port = htons(SSDP_PORT);
    bind_addr_ssdp.sin_addr.s_addr = htonl(INADDR_ANY);
    
    bind_addr_ack.sin_family = AF_INET;
    bind_addr_ack.sin_port = htons(ACK_PORT);
    bind_addr_ack.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sckt_ssdp, (struct sockaddr*) &bind_addr_ssdp, sizeof(bind_addr_ssdp)) < 0) {
        std::cerr << "Error binding SSDP port to socket: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    if (bind(sckt_ack, (struct sockaddr*) &bind_addr_ack, sizeof(bind_addr_ack)) < 0) {
        std::cerr << "Error binding ACK port to socket: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    struct ip_mreq mreq;
    std::memset(&mreq, 0, sizeof(mreq));
    
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    response = setsockopt(sckt_ssdp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    if (response < 0) {
        std::cerr << "Error setting socket option: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    std::vector<char*> discovered_devices;
    
    std::cout << "Discovering Devices..." << std::endl;
    
    bool discovering = true;
    std::thread receive_thread_ssdp(&Network::receive_ssdp_message, this, sckt_ssdp, std::ref(discovered_devices), std::ref(discovering));
    
    const char* message =   "M-SEARCH * HTTP/1.1\r\n"
                            "HOST: 239.255.255.250:1900\r\n"
                            "MAN: \"ssdp:notify\"\r\n"
                            "ST: ssdp:all\r\n"
                            "MX: 3\r\n"
                            "\r\n";
    
    // discovery phase
    for (int _=0; _!=10; _++) {
        send_ssdp_message(sckt_ssdp, message);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    discovering = false;
    
    receive_thread_ssdp.join();
    close(sckt_ssdp);
    
    std::cout << "Discovered " << discovered_devices.size() << " devices." << std::endl;
    std::cout << "Acknowledging Devices..." << std::endl;
    
    std::vector<char*> pending_acknowledgements;
    std::vector<char*> acknowledged_devices;
    
    // acknowledgement phase
    bool acknowledging = true;
    
    std::thread receive_thread_ack(&Network::receive_ack_message, this, sckt_ack, std::ref(pending_acknowledgements), std::ref(acknowledged_devices), std::ref(acknowledging));
    
    while (acknowledged_devices.size() != n_devices-1) {
        for (auto x : discovered_devices) {
            if (std::find_if(acknowledged_devices.begin(), acknowledged_devices.end(), [x](const char* c) { return std::string(x) == std::string(c); }) == acknowledged_devices.end()) {
                std::this_thread::sleep_for(std::chrono::milliseconds((std::rand() % 1000) + 100));
                send_ack_message(sckt_ack, inet_addr(x), "ACK");
            }
        }
    }
    
    acknowledging = false;
    
    receive_thread_ack.join();
    
    close(sckt_ack);
    
    net_config.devices = new char*[acknowledged_devices.size()];
    
    std::cout << "Found " << acknowledged_devices.size() << " devices:" << std::endl;
    for (int i=0; i!=acknowledged_devices.size(); i++) {
        auto x = acknowledged_devices[i];
        net_config.devices[i] = new char[INET_ADDRSTRLEN];
        memcpy(net_config.devices[i], x, INET_ADDRSTRLEN);
        std::cout << "\t" << net_config.devices[i] << std::endl;
        
    }
}

Network::NetworkConfig* Network::get_network_config() {
    return &net_config;
}
