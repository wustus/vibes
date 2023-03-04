
#include "network.h"

Network::Network(int NUMBER_OF_DEVICES) {
    
    Network::NUMBER_OF_DEVICES = NUMBER_OF_DEVICES;
    
    RECEIVING_BUFFER = new char*[BUFFER_SIZE];
    for (int i=0; i!=BUFFER_SIZE; i++) {
        RECEIVING_BUFFER[i] = new char[MESSAGE_SIZE];
        RECEIVING_BUFFER[i][0] = '\0';
    }
    
    net_config.devices = new char*[NUMBER_OF_DEVICES-1];
    for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
        net_config.devices[i] = new char[INET_ADDRSTRLEN];
        net_config.devices[i][0] = '\0';
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

void Network::split_buffer_message(char *&addr, char *&msg, char* buffer_msg) {
    
    std::string buffer = std::string(buffer_msg);
    int delimiter_index = static_cast<int>(buffer.find_first_of("::"));
    
    std::string temp_addr = buffer.substr(0, delimiter_index);
    std::string temp_msg = buffer.substr(delimiter_index+2, buffer.size());
    
    addr = new char[INET_ADDRSTRLEN];
    msg = new char[temp_msg.size()];
    
    std::memcpy(addr, temp_addr.c_str(), INET_ADDRSTRLEN);
    std::memcpy(msg, temp_msg.c_str(), temp_msg.size());
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

bool Network::send_message(int sckt, const char* addr, int port, const char* msg, short timeout=3) {
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_port = htons(port);
    
    if (sendto(sckt, (void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Error while sending message: " << std::strerror(errno) << std::endl;
    }

    if (std::strcmp(msg, "ACK") == 0) {
        return true;
    }
    
    if (!listen_for_ack(addr)) {
        
        if (timeout == 0) {
            return false;
        }
        
        send_message(sckt, addr, port, msg, timeout-1);
    }
    
    return true;
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
    
    std::vector<char*> discovered_devices;
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
    while (discovered_devices.size() != NUMBER_OF_DEVICES-1) {
        sending_threads.emplace_back([this, message]() { send_message(ssdp_sckt, SSDP_ADDR, SSDP_PORT, message); });
        for (int i=0; i!=current_index; i++) {
            if (*RECEIVING_BUFFER[i] != '\0') {
                
                char* addr;
                char* msg;
                
                split_buffer_message(addr, msg, RECEIVING_BUFFER[i]);
                
                if (addr == std::string(SSDP_ADDR) || addr == std::string(local_addr) || addr == std::string(ROUTER_ADDR)) {
                    continue;
                }
                
                if ((std::strcmp(msg, "ACK") == 0 || std::strcmp(msg, "BUDDY") == 0) && std::find_if(discovered_devices.begin(), discovered_devices.end(), [addr](char* c) {
                    return std::string(addr) == std::string(c);
                }) == discovered_devices.end()) {
                    discovered_devices.push_back(addr);
                } else {
                    sending_threads.emplace_back([this, addr]() { send_message(ssdp_sckt, addr, ACK_PORT, "BUDDY"); });
                }
                
                delete[] msg;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    for (int i=0; i!=sending_threads.size(); i++) {
        sending_threads[i].join();
    }
    
    discovering = false;
    
    receive_thread_ssdp.join();
    close(ssdp_sckt);
    
    std::cout << "Discovered " << discovered_devices.size() << " devices." << std::endl;
    
    for (int i=0; i!=discovered_devices.size(); i++) {
        memcpy(net_config.devices[i], discovered_devices[i], INET_ADDRSTRLEN);
        std::cout << "\t" << net_config.devices[i] << std::endl;
        
    }
}

bool Network::challenge_handler(char*& challenger, bool& found_challenger) {
    
    while (!found_challenger) {
        
        for (int i=0; i!=BUFFER_SIZE; i++) {
            
            if (*RECEIVING_BUFFER[i] == '\0') {
                break;
            }
            
            char* addr;
            char* msg;
            
            split_buffer_message(addr, msg, RECEIVING_BUFFER[i]);
            
            // pending challenge
            if (challenger != nullptr) {
                if (std::strcmp(addr, challenger) == 0) {
                    if (std::strcmp(msg, "ACC") == 0) {
                        found_challenger = true;
                        challenger = new char[INET_ADDRSTRLEN];
                        std::memcpy(challenger, addr, INET_ADDRSTRLEN);
                    } else if (std::strcmp(msg, "DEC") == 0) {
                        challenger = nullptr;
                        
                        // just for tests i dont like it though
                        if (NUMBER_OF_DEVICES == 3) {
                            delete[] addr;
                            delete[] msg;
                            
                            return false;
                        }
                    } else if (std::strcmp(msg, "CHLG") == 0) {
                        found_challenger = true;
                    }
                }
            } else {
                if (std::strcmp(msg, "CHLG") == 0) {
                    if (send_message(chlg_sckt, addr, CHLG_PORT, "ACC")) {
                        found_challenger = true;
                        challenger = new char[INET_ADDRSTRLEN];
                        std::memcpy(challenger, addr, INET_ADDRSTRLEN);
                    }
                }
            }
            
            
            delete[] addr;
            delete[] msg;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    return true;
}

char* Network::find_challenger(char** game_status) {
    
    bool receiving = true;
    bool found_challenger = false;
    bool waiting_for_challenge = false;
    
    char* challenger = nullptr;
    
    std::thread recv_thread([this, &receiving]() {
        receive_messages(chlg_sckt, receiving);
    });
    
    std::thread handler_thread([this, &challenger, &found_challenger, &waiting_for_challenge]() { waiting_for_challenge = !challenge_handler(challenger, found_challenger); });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));
    
    int current_addr = 0;
    int pending_timeout = 10;
    
    while (!found_challenger && !waiting_for_challenge) {
        
        if (challenger == nullptr) {
            char* chlg_addr = net_config.devices[current_addr];
            if (send_message(chlg_sckt, chlg_addr, CHLG_PORT, "CHLG")) {
                challenger = new char[INET_ADDRSTRLEN];
                std::memcpy(challenger, chlg_addr, INET_ADDRSTRLEN);
            } else {
                pending_timeout = 0;
            }
        } else {
            pending_timeout--;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        if (pending_timeout == 0) {
            current_addr = ++current_addr % NUMBER_OF_DEVICES;
            pending_timeout = 10;
        }
    }
    
    receiving = false;
    
    recv_thread.join();
    handler_thread.join();
    
    return challenger;
}

void Network::game_status_listener(char**& game_status, bool& listening) {
    
    while (listening) {
        
        for (int i=0; i!=BUFFER_SIZE; i++) {
            if (*RECEIVING_BUFFER[i] == '\0') {
                continue;
            }
                
            char* src_addr;
            char* temp_msg;
            char* ref_addr;
            char* msg;
            
            // get source addr (the one that has won or lost)
            split_buffer_message(src_addr, temp_msg, RECEIVING_BUFFER[i]);
            
            // get opponent address
            split_buffer_message(ref_addr, msg, temp_msg);
            
            if (std::strcmp(msg, "WON") != 0) {
                continue;
            }
            
            int c = 0;
            bool contains_message = false;
            
            while (game_status[c]) {
                
                if (std::strcmp(game_status[c], RECEIVING_BUFFER[i]) == 0) {
                    contains_message = true;
                    break;
                }
                
                c++;
            }
            
            if (contains_message) {
                break;
            }
            
            std::memcpy(game_status[c], RECEIVING_BUFFER[c], std::strlen(RECEIVING_BUFFER[c]));
        }
    }
}


void Network::start_game(char* addr) {
    
    std::memset(&game, 0, sizeof(game));
    
    game.opponent_addr = new char[INET_ADDRSTRLEN];
    std::memcpy(game.opponent_addr, addr, INET_ADDRSTRLEN);
    
    for (int i=0; i!=0; i++) {
        game.played_moves[i] = -1;
    }
    
    game.is_game_live = true;
    
    game.game_thread = std::thread(&Network::receive_messages, this, chlg_sckt, std::ref(game.is_game_live));
}

short Network::receive_move() {
    
    bool received = false;
    short move;
    
    while (!received) {
        for (int i=0; i!=BUFFER_SIZE; i++) {
            if (*RECEIVING_BUFFER[i] == '\0') {
                break;
            }
            
            char* addr;
            char* msg;
            
            split_buffer_message(addr, msg, RECEIVING_BUFFER[i]);
            
            if (std::strcmp(addr, game.opponent_addr) == 0) {
                if (std::strncmp("MOVE", msg, 4) == 0) {
                    char* tmp = new char[std::strlen(msg)];
                    short move;
                    std::memcpy(tmp, msg+5, sizeof(short));
                    std::memcpy(&move, tmp, sizeof(short));
                    
                    received = true;
                    
                    delete[] tmp;
                }
            }
            
            delete[] msg;
            delete[] addr;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    return move;
}

void Network::make_move(short m) {
    
    char* msg = new char[6 + sizeof(short)];
    
    std::memcpy(msg, "MOVE ", 5);
    std::memcpy(msg+5, &m, sizeof(m));
    
    while (!send_message(chlg_sckt, game.opponent_addr, CHLG_PORT, msg)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    std::cout << "MOVE PLAYED: " << m << std::endl;
    std::cout << "MOVE PLAYED: " << msg << std::endl << std::endl;
    
    for (int i=0; i!=8; i++) {
        if (game.played_moves[i] == -1) {
            continue;
        }
        
        game.played_moves[i] = m;
        break;
    }
    
    delete[] msg;
}

void Network::end_game() {
    
    game.is_game_live = false;
    delete[] game.opponent_addr;
    std::memset(&game, 0, sizeof(game));
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

int Network::get_buffer_size() {
    return BUFFER_SIZE;
}

int Network::get_message_size() {
    return MESSAGE_SIZE;
}

char** Network::get_receiving_buffer() {
    return RECEIVING_BUFFER;
}

int Network::get_current_index() {
    return current_index;
}

Network::NetworkConfig* Network::get_network_config() {
    return &net_config;
}
