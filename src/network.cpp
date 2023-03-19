
#include "network.h"

Network::Network(int NUMBER_OF_DEVICES) : net_config(NUMBER_OF_DEVICES), thread_pool(20) {
    
    Network::NUMBER_OF_DEVICES = NUMBER_OF_DEVICES;
    
    RECEIVING_BUFFER = new char*[MSG_BUFFER_SIZE];
    ACK_BUFFER = new char*[MSG_BUFFER_SIZE];
    CHLG_BUFFER = new char*[MSG_BUFFER_SIZE];
    GAME_BUFFER = new char*[MSG_BUFFER_SIZE];
    
    for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
        RECEIVING_BUFFER[i] = new char[MESSAGE_SIZE];
        ACK_BUFFER[i] = new char[MESSAGE_SIZE];
        CHLG_BUFFER[i] = new char[MESSAGE_SIZE];
        GAME_BUFFER[i] = new char[MESSAGE_SIZE];
        
        
        RECEIVING_BUFFER[i][0] = '\0';
        ACK_BUFFER[i][0] = '\0';
        CHLG_BUFFER[i][0] = '\0';
        GAME_BUFFER[i][0] = '\0';
    }
    
    // initialize sockets
    ssdp_sckt = create_udp_socket(SSDP_PORT);
    ack_sckt = create_udp_socket(ACK_PORT);
    disc_sckt = create_udp_socket(DISC_PORT);
    chlg_sckt = create_udp_socket(CHLG_PORT);
    game_sckt = create_udp_socket(GAME_PORT);
    ntp_sckt = create_udp_socket(NTP_PORT);
    
    // join ssdp multicast group
    int response;
    
    struct ip_mreq mreq;
    std::memset(&mreq, 0, sizeof(mreq));
    
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    response = setsockopt(ssdp_sckt, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    if (response < 0) {
        std::cerr << "Error setting Multicast socket option: " << std::strerror(errno) << std::endl;
        exit(1);
    }

    transmission_thread_active = true;
    transmission_thread = std::thread(&Network::transmission_handler, this);
    
    ack_thread_active = true;
    ack_thread = std::thread(&Network::ack_listener, this);
}

Network::~Network() {
    
    transmission_thread_active = false;
    ack_thread_active = false;
    chlg_thread_active = false;
    ntp_thread_active = false;
    
    close(ssdp_sckt);
    close(ack_sckt);
    close(disc_sckt);
    close(chlg_sckt);
    close(game_sckt);
    close(ntp_sckt);
    
    if (transmission_thread.joinable()) {
        transmission_thread.join();
    }
    
    if (ack_thread.joinable()) {
        ack_thread.join();
    }
    
    if (chlg_thread.joinable()) {
        chlg_thread.join();
    }
    
    if (ntp_thread.joinable()) {
        ntp_thread.join();
    }
    
    for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
        delete[] RECEIVING_BUFFER[i];
        delete[] ACK_BUFFER[i];
        delete[] CHLG_BUFFER[i];
        delete[] GAME_BUFFER[i];
    }
    
    delete[] RECEIVING_BUFFER;
    delete[] ACK_BUFFER;
    delete[] CHLG_BUFFER;
    delete[] GAME_BUFFER;
    
    message_buffer.clear();
}

int Network::create_udp_socket(int port) {
    
    int sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
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
                    net_config.set_address(ip_addr);
                    return;
                }
            #endif
            
            #ifdef __unix
                if (std::strcmp(ifaddr_iter->ifa_name, "wlan0") == 0) {
                    net_config.set_address(ip_addr);
                    return;
                }
            #endif
        }
    }
}

void Network::split_buffer_message(char *&val1, char *&val2, char* msg) {
    
    std::string buffer = std::string(msg);
    int delimiter_index = static_cast<int>(buffer.find_first_of("::"));
    
    std::string temp_val1 = buffer.substr(0, delimiter_index);
    std::string temp_val2 = buffer.substr(delimiter_index+2, buffer.size());
    
    size_t val1_size = INET_ADDRSTRLEN > temp_val1.size() ? INET_ADDRSTRLEN : temp_val1.size();
    size_t val2_size = INET_ADDRSTRLEN > temp_val2.size() ? INET_ADDRSTRLEN : temp_val2.size();
    
    val1 = new char[val1_size];
    val2 = new char[val2_size];
    
    std::memcpy(val1, temp_val1.c_str(), val1_size);
    std::memcpy(val2, temp_val2.c_str(), val2_size);
}

void Network::append_to_buffer(char* addr, char* message, char**& buffer, int& counter) {
    
    if (std::strlen(addr) + 2 + std::strlen(message) > MESSAGE_SIZE) {
        char error_msg[std::strlen(addr) + std::strlen(message) + MESSAGE_SIZE];
        std::snprintf(error_msg, sizeof(error_msg), "Message too long for Buffer: \n\t%s::%s (%zu + 2 + %zu) \ntoo long for MESSAGE_SIZE=%d.", addr, message, std::strlen(addr), std::strlen(message), MESSAGE_SIZE);
        throw std::length_error(error_msg);
    }
    
    char* buffer_msg = new char[MESSAGE_SIZE];
    std::snprintf(buffer_msg, MESSAGE_SIZE, "%s::%s", addr, message);
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    
    if (*buffer[counter] != '\0') {
        delete[] buffer[counter];
        buffer[counter] = new char[MESSAGE_SIZE];
    }
    
    std::memcpy(buffer[counter], buffer_msg, MESSAGE_SIZE);
    counter = ++counter % MSG_BUFFER_SIZE;
    
    delete[] buffer_msg;
}

void Network::flush_buffer(char**& buffer, int& counter) {
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
        delete[] buffer[i];
        buffer[i] = new char[MESSAGE_SIZE];
    }
    
    counter = 0;
}

uint16_t Network::checksum(char* data) {
    
    uint8_t sum1 = 0;
    uint8_t sum2 = 0;
    
    for (int i=0; i!=std::strlen(data); i++) {
        sum1 = (sum1 + data[i]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    
    return (sum2 << 8) | sum1;
}

void Network::ack_listener() {
    
    while (ack_thread_active) {
        
        char* recv_buffer = new char[MESSAGE_SIZE];
        std::memset(recv_buffer, 0, MESSAGE_SIZE);
        
        sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(ack_sckt, recv_buffer, MESSAGE_SIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Failed to receive ACK message: " << std::strerror(errno) << std::endl;
            }
            
            delete[] recv_buffer;
            continue;
        }
        
        char* addr;
        char* chksum;
        
        split_buffer_message(addr, chksum, recv_buffer);
        append_to_buffer(addr, chksum, ACK_BUFFER, current_ack_index);
        
        delete[] recv_buffer;
        delete[] addr;
        delete[] chksum;
    }
}

void Network::ack_handler(Message message) {
    
    char* addr = message.addr;
    char* msg = message.msg;
    int timeout = message.timeout;
    
    if (!listen_for_ack(addr, msg) && timeout > 0) {
        int sckt = message.sckt;
        int port = message.port;
        
        send_message(sckt, addr, port, msg, timeout-1);
    }
}

bool Network::listen_for_ack(char* addr, char* msg) {
    
    std::chrono::duration<double> duration{1.5};
    
    std::chrono::time_point start_time = std::chrono::steady_clock::now();
    
    int c = 0;
    
    uint16_t calc_chksum = checksum(msg);
    
    while (std::chrono::steady_clock::now() - start_time < duration) {
        
        if (*ACK_BUFFER[c] == '\0') {
            c = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        
        char* buffer_msg = new char[MESSAGE_SIZE];
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            std::memcpy(buffer_msg, ACK_BUFFER[c], MESSAGE_SIZE);
        }
        
        char* recv_addr;
        char* recv_msg;
        
        split_buffer_message(recv_addr, recv_msg, buffer_msg);
        
        delete[] buffer_msg;
        
        if (std::strcmp(recv_addr, addr) == 0) {
            uint16_t chksum = static_cast<uint16_t>(strtoul(recv_msg, nullptr, 10));
            
            if (chksum == calc_chksum) {
                delete[] recv_addr;
                delete[] recv_msg;
                
                return true;
            }
        }
        
        c = ++c % MSG_BUFFER_SIZE;
        
        delete[] recv_addr;
        delete[] recv_msg;
    }
    
    return false;
}

void Network::send_ack(char* addr, char* msg) {
    
    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_port = htons(ACK_PORT);
    
    size_t ack_msg_size = INET_ADDRSTRLEN + 2 + sizeof(uint16_t);
    char* ack_msg = new char[ack_msg_size];
    
    uint16_t chksum = checksum((char*) msg);
    
    std::snprintf(ack_msg, ack_msg_size, "%s::%d", net_config.address, chksum);
    
    if (sendto(ack_sckt, (void*)(intptr_t) ack_msg, ack_msg_size, 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
        std::cerr << "Failed sending ACK for message: \n\t" << msg << std::endl << "Error: " << std::strerror(errno) << std::endl;
    }
    
    delete[] ack_msg;
}

void Network::transmission_handler() {
    
    while (transmission_thread_active) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (message_buffer.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        std::lock_guard<std::mutex> lock(sender_mutex);
        Message message = message_buffer.front();
        message_buffer.pop_front();
        
        int sckt = message.sckt;
        char* addr  = message.addr;
        int port = message.port;
        char* msg = message.msg;
        
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr(addr);
        dest_addr.sin_port = htons(port);
        
        if (sendto(sckt, (void*)(intptr_t) msg, std::strlen(msg), 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
            std::cerr << "Error while sending message: " << std::strerror(errno) << std::endl;
            continue;
        }
        
        if (thread_pool.is_running()) {
            thread_pool.enqueue_task([this, message]() { ack_handler(message); });
        }
    }
}

void Network::send_message(int sckt, const char* addr, int port, const char* msg, short timeout=3) {
    
    Message message(sckt, (char*) addr, port, (char*) msg, timeout);
    
    std::lock_guard<std::mutex> lock(sender_mutex);
    message_buffer.push_back(message);
}

void Network::receive_messages(int sckt, bool& receiving, char**& buffer, int& counter) {
    
    while (receiving) {
        
        char* recv_buffer = new char[MESSAGE_SIZE];
        memset(recv_buffer, 0, MESSAGE_SIZE);
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(sckt, recv_buffer, MESSAGE_SIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error while receiving message: " << std::strerror(errno) << std::endl;
            }
            delete[] recv_buffer;
            continue;
        }
        
        char* device = new char[INET_ADDRSTRLEN];
        std::memset(device, 0, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(src_addr.sin_addr), device, INET_ADDRSTRLEN);
        
        send_ack(device, recv_buffer);
        append_to_buffer(device, recv_buffer, buffer, counter);
        
        delete[] device;
        delete[] recv_buffer;
    }
}

void Network::discover_devices() {
    
    std::vector<char*> discovered_devices;
    
    char* local_addr = net_config.address;
    
    std::cout << "Discovering Devices..." << std::endl;
    
    bool discovering = true;
    std::thread receive_thread_ssdp(&Network::receive_messages, this, ssdp_sckt, std::ref(discovering), std::ref(RECEIVING_BUFFER), std::ref(current_index));
    std::thread receive_thread_disc(&Network::receive_messages, this, disc_sckt, std::ref(discovering), std::ref(RECEIVING_BUFFER), std::ref(current_index));
    
    const char* message =   "M-SEARCH * HTTP/1.1\r\n"
                            "HOST: 239.255.255.250:1900\r\n"
                            "MAN: \"ssdp:notify\"\r\n"
                            "ST: ssdp:all\r\n"
                            "MX: 3\r\n"
                            "\r\n";
    
    // discovery phase
    while (discovered_devices.size() != NUMBER_OF_DEVICES-1) {
        send_message(ssdp_sckt, SSDP_ADDR, SSDP_PORT, message);
        
        int max_index = std::max(current_index, current_ack_index);
        for (int i=0; i!=max_index; i++) {
            if (*RECEIVING_BUFFER[i] != '\0') {
                
                char* buffer_msg = new char[MESSAGE_SIZE];
                
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    std::memcpy(buffer_msg, RECEIVING_BUFFER[i], MESSAGE_SIZE);
                }
                
                char* addr;
                char* msg;
                
                split_buffer_message(addr, msg, buffer_msg);
                
                delete[] buffer_msg;
                
                if (std::strncmp(addr, SSDP_ADDR, INET_ADDRSTRLEN) != 0 && std::strncmp(addr, local_addr, INET_ADDRSTRLEN) != 0 && std::strncmp(addr, ROUTER_ADDR, INET_ADDRSTRLEN) != 0) {
                    if (std::strncmp(msg, "BUDDY", 5) == 0 && std::find_if(discovered_devices.begin(), discovered_devices.end(), [addr](char* c) {
                        return std::strncmp(addr, c, INET_ADDRSTRLEN) == 0;
                    }) == discovered_devices.end()) {
                        discovered_devices.push_back(addr);
                    } else {
                        send_message(disc_sckt, addr, DISC_PORT, "BUDDY");
                        delete[] addr;
                    }
                } else {
                    delete[] addr;
                }
                
                delete[] msg;
            }
            
            if (*ACK_BUFFER[i] != '\0') {
                
                char* buffer_msg = new char[MESSAGE_SIZE];
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    std::memcpy(buffer_msg, ACK_BUFFER[i], MESSAGE_SIZE);
                }
                
                char* addr;
                char* msg;
                
                split_buffer_message(addr, msg, buffer_msg);
                
                delete[] buffer_msg;
                delete[] msg;
                
                if (std::strncmp(addr, SSDP_ADDR, INET_ADDRSTRLEN) != 0 && std::strncmp(addr, local_addr, INET_ADDRSTRLEN) != 0 && std::strncmp(addr, ROUTER_ADDR, INET_ADDRSTRLEN) != 0) {
                    if (std::find_if(discovered_devices.begin(), discovered_devices.end(), [addr](char* c) {
                        return std::strncmp(addr, c, INET_ADDRSTRLEN) == 0;
                    }) == discovered_devices.end()) {
                        discovered_devices.push_back(addr);
                        continue;
                    }
                }
                
                delete[] addr;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    std::cout << "Finishd Searching." << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(sender_mutex);
        message_buffer.clear();
    }
    
    // flush and start up again
    thread_pool.stop_and_flush_threads();
    thread_pool.start();
    
    discovering = false;
    
    receive_thread_ssdp.join();
    receive_thread_disc.join();
    close(ssdp_sckt);
    close(disc_sckt);
    
    std::cout << "Discovered " << discovered_devices.size() << " devices." << std::endl;
    
    for (int i=0; i!=NUMBER_OF_DEVICES-1; i++) {
        net_config.add_device(discovered_devices[i]);
        std::cout << "\t" << net_config.devices[i] << std::endl;
        delete[] discovered_devices[i];
    }
}

void Network::start_challenge_listener() {
    chlg_thread_active = true;
    chlg_thread = std::thread(&Network::receive_messages, this, chlg_sckt, std::ref(chlg_thread_active), std::ref(CHLG_BUFFER), std::ref(current_chlg_index));
}

char* Network::find_challenger(char**& game_status) {
    
    /*
        if game has been announced and no winner yet, wait for announcement
     */
    
    bool initial = true;
    bool wait_for_game = false;
    
    while (initial || wait_for_game) {
        static int game_index = -1;
        initial = false;
        
        char* player1 = new char[INET_ADDRSTRLEN];
        char* player2 = new char[INET_ADDRSTRLEN];
        *player1 = '\0';
        *player2 = '\0';
        
        for (int i=0; i!=16; i++) {
            if (*game_status[i] == '\0') {
                break;
            }
            
            char* msg_bufffer = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(msg_bufffer, game_status[i], MESSAGE_SIZE);
            }
            
            char* addr1;
            char* addr2;
            char* tmp;
            char* msg;
            
            split_buffer_message(addr1, tmp, msg_bufffer);
            split_buffer_message(addr2, msg, tmp);
            
            if (std::strncmp(msg, "GAME", 4) == 0) {
                if (*player1 == '\0') {
                    wait_for_game = true;
                    std::memcpy(player1, addr1, INET_ADDRSTRLEN);
                    std::memcpy(player2, addr2, INET_ADDRSTRLEN);
                } else if (std::strncmp(player1, addr2, INET_ADDRSTRLEN) != 0 && std::strncmp(player2, addr1, INET_ADDRSTRLEN) != 0) {
                    game_index = i;
                }
            }
            
            if (wait_for_game) {
                if ((std::strncmp(player1, addr1, INET_ADDRSTRLEN) == 0 && std::strncmp(player2, addr2, INET_ADDRSTRLEN) == 0)
                    || (std::strncmp(player1, addr2, INET_ADDRSTRLEN) == 0 && std::strncmp(player2, addr1, INET_ADDRSTRLEN) == 0)) {
                    if (std::strncmp(msg, "WIN", 3) == 0 || std::strncmp(msg, "LOSE", 4) == 0) {
                        if (game_index == -1) {
                            wait_for_game = false;
                        } else {
                            i = game_index;
                        }
                        
                        delete[] player1;
                        delete[] player2;
                        
                        player1 = new char[INET_ADDRSTRLEN];
                        player2 = new char[INET_ADDRSTRLEN];
                        *player1 = '\0';
                        *player2 = '\0';
                    }
                }
            }
            
            delete[] msg_bufffer;
            delete[] addr1;
            delete[] addr2;
            delete[] tmp;
            delete[] msg;
        }
        
        delete[] player1;
        delete[] player2;
    }
    
    
    char** players = new char*[NUMBER_OF_DEVICES];
    char** losers = new char*[NUMBER_OF_DEVICES];
    
    for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
        players[i] = new char[INET_ADDRSTRLEN];
        losers[i] = new char[INET_ADDRSTRLEN];
        
        *players[i] = '\0';
        *losers[i] = '\0';
    }
    
    // populate
    std::memcpy(players[0], net_config.address, INET_ADDRSTRLEN);
    for (int i=1; i!=NUMBER_OF_DEVICES; i++) {
        std::memcpy(players[i], net_config.devices[i-1], INET_ADDRSTRLEN);
    }

    int c = 0;
    
    for (int i=0; i!=16; i++) {
        
        if (*game_status[i] == '\0') {
            break;
        }
        
        char* buffer_msg = new char[128];
        {
            std::lock_guard<std::mutex> lock(var_mutex);
            std::memcpy(buffer_msg, game_status[i], 128);
        }
        
        char* winner;
        char* tmp_msg;
        char* loser;
        char* msg;
        
        split_buffer_message(winner, tmp_msg, buffer_msg);
        split_buffer_message(loser, msg, tmp_msg);
        
        if (std::strncmp(msg, "WIN", 3) == 0) {
            std::memcpy(players[c], winner, INET_ADDRSTRLEN);
            std::memcpy(losers[c], loser, INET_ADDRSTRLEN);
            c++;
        }
        
        delete[] buffer_msg;
        delete[] winner;
        delete[] loser;
        delete[] tmp_msg;
        delete[] msg;
    }

    // sort out
    for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
        if (*losers[i] == '\0') {
            break;
        }
        
        for (int j=0; j!=NUMBER_OF_DEVICES; j++) {
            if (std::strncmp(players[j], losers[i], INET_ADDRSTRLEN) == 0) {
                *players[j] = '\0';
            }
        }
    }
    
    size_t still_player_size = 0;
    for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
        if (*players[i] != '\0') {
            still_player_size++;
        }
    }
    
    char** still_player = new char*[still_player_size];
    c = 0;
    
    for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
        if (*players[i] != '\0') {
            still_player[c] = new char[INET_ADDRSTRLEN];
            std::memcpy(still_player[c], players[i], INET_ADDRSTRLEN);
            c++;
        }
    }
    
    char* challenger = nullptr;
    
    std::function<bool(char*, char*)> lex_compare = [](char* c1, char* c2) {
        return std::strcmp(c1, c2) < 0;
    };
    
    while (challenger == nullptr) {
        
        // sort and create list of distances
        std::sort(still_player, still_player + still_player_size, lex_compare);
        int distances[still_player_size-1];
        
        for (int i=0; i!=still_player_size-1; i++) {
            distances[i] = abs(std::strcmp(still_player[i], still_player[i+1]));
        }
        
        int my_index = [this, &still_player, &still_player_size]() { for (int i=0; i!=still_player_size; i++) {if (std::strncmp(still_player[i], net_config.address, INET_ADDRSTRLEN) == 0) { return i; }}} ();
        
        int nearest_neighbors[still_player_size];
        
        // copy nn
        for (int i=0; i!=still_player_size; i++) {
            if (i == 0) {
                nearest_neighbors[i] = i+1;
            } else if (i == still_player_size-1) {
                nearest_neighbors[i] = i-1;
            } else {
                nearest_neighbors[i] = distances[i-1] < distances[i] ? i-1 : i+1;
            }
        }
        
        int nn = nearest_neighbors[my_index];
        if (my_index == nearest_neighbors[nn]) {
            challenger = new char[INET_ADDRSTRLEN];
            std::memcpy(challenger, still_player[nn], INET_ADDRSTRLEN);
        } else {
            int temp = nearest_neighbors[nn];
            delete[] still_player[nn];
            delete[] still_player[temp];
            
            still_player[nn] = nullptr;
            still_player[temp] = nullptr;
            
            int new_index = 0;
            for (int i=0; i!=still_player_size; i++) {
                if (still_player[i] != nullptr) {
                    still_player[new_index++] = still_player[i];
                }
            }
            
            still_player_size -= 2;
        }
        
        if (still_player_size == 1) {
            return nullptr;
        }
    }
    
    return challenger;
}

void Network::game_status_listener(char**& game_status, bool& listening) {
    
    while (listening) {
        
        for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
            if (*CHLG_BUFFER[i] == '\0') {
                continue;
            }
            
            char* buffer_msg = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(buffer_msg, CHLG_BUFFER[i], MESSAGE_SIZE);
            }
                
            char* src_addr;
            char* temp_msg;
            char* ref_addr;
            char* msg;
            
            // get source addr (the one that has won or lost)
            split_buffer_message(src_addr, temp_msg, buffer_msg);
             
            
            
            // get opponent address
            split_buffer_message(ref_addr, msg, temp_msg);
            
            delete[] src_addr;
            delete[] temp_msg;
            delete[] ref_addr;
            
            if (std::strncmp(msg, "WIN", 3) == 0 || std::strncmp(msg, "LOSE", 4) == 0 || std::strncmp(msg, "WAIT", 4) == 0 || std::strncmp(msg, "GAME", 4) == 0) {
                delete[] msg;
            
                int c = 0;
                
                while (*game_status[c] != '\0') {
                    
                    if (std::strcmp(game_status[c], buffer_msg) != 0) {
                        break;
                    }
                    
                    c = ++c % 16;
                }
                
                std::memcpy(game_status[c], buffer_msg, MESSAGE_SIZE);
            }
            
            delete[] msg;
            delete[] buffer_msg;
        }
    }
}

void Network::listen_for_ready(char* addr, bool& listening) {

    while (listening) {
        
        for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
            
            if (*CHLG_BUFFER[i] == '\0') {
                break;
            }
            
            char* buffer_msg = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(buffer_msg, CHLG_BUFFER[i], MESSAGE_SIZE);
            }
            
            char* recv_addr;
            char* msg;
            
            split_buffer_message(recv_addr, msg, buffer_msg);
            
            delete[] buffer_msg;
            
            if (std::strcmp(recv_addr, addr) == 0 && std::strncmp(msg, "READY", 5) == 0) {
                listening = false;
                
                delete[] recv_addr;
                delete[] msg;
                break;
            }
            
            delete[] recv_addr;
            delete[] msg;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Network::wait_until_ready(char *addr) {
    
    bool listening = true;
    
    std::thread ready_listener(&Network::listen_for_ready, this, addr, std::ref(listening));
    
    send_message(chlg_sckt, addr, CHLG_PORT, "READY");
    
    while (listening) {
        send_message(chlg_sckt, addr, CHLG_PORT, "READY");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ready_listener.join();
}

void Network::wait_for_challenge(char**& game_status) {
    
    bool challenge_found = false;
    while (!challenge_found) {
        for (int i=0; i!=16; i++) {
            if (*game_status[i] == '\0') {
                break;
            }
            
            char* msg_buffer = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(msg_buffer, game_status[i], MESSAGE_SIZE);
            }
            
            char* addr1;
            char* addr2;
            char* tmp;
            char* msg;
            
            split_buffer_message(addr1, tmp, msg_buffer);
            split_buffer_message(addr2, msg, tmp);
            
            if (std::strncmp(msg, "WIN", 3) == 0 || std::strncmp(msg, "LOSE", 4) == 0) {
                challenge_found = true;
            }
            
            delete[] msg_buffer;
            delete[] addr1;
            delete[] addr2;
            delete[] tmp;
            delete[] msg;
        }
    }
}

void Network::start_game(char* addr) {
    
    std::memset(&game, 0, sizeof(game));
    
    game.opponent_addr = new char[INET_ADDRSTRLEN];
    std::memcpy(game.opponent_addr, addr, INET_ADDRSTRLEN);
    
    for (int i=0; i!=9; i++) {
        game.played_moves[i] = -1;
    }
    
    game.is_game_live = true;
    game.game_thread = std::thread(&Network::receive_messages, this, game_sckt, std::ref(game.is_game_live), std::ref(GAME_BUFFER), std::ref(current_game_index));
    
    // new random seed
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

void Network::flush_game_buffer() {
    flush_buffer(GAME_BUFFER, current_game_index);
}

short Network::receive_move() {
    
    while (true) {
        for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
            if (*GAME_BUFFER[i] == '\0') {
                break;
            }
            
            char* buffer_msg = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(buffer_msg, GAME_BUFFER[i], MESSAGE_SIZE);
            }
            
            char* addr;
            char* msg;
            
            split_buffer_message(addr, msg, buffer_msg);
            
            delete[] buffer_msg;
            
            if (std::strcmp(addr, game.opponent_addr) == 0) {
                if (std::strncmp("MOVE", msg, 4) == 0) {
                    short move;
                    bool new_move = false;
                    short new_move_index = -1;
                    
                    sscanf(msg+5, "%hd", &move);
                    
                    for (short j=0; j!=8; j++) {
                        if (game.played_moves[j] == move) {
                            break;
                        } else if (game.played_moves[j] == -1) {
                            new_move = true;
                            new_move_index = j;
                            break;
                        }
                    }
                    
                    if (new_move) {
                        delete[] msg;
                        delete[] addr;

                        game.played_moves[new_move_index] = move;
                        
                        return move;
                    }
                }
            }
            
            delete[] msg;
            delete[] addr;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return -1;
}

void Network::make_move(short m) {
    
    char* msg = new char[5 + sizeof(short)];
    std::snprintf(msg, 5 + sizeof(short), "MOVE %hu", m);
    
    send_message(game_sckt, game.opponent_addr, GAME_PORT, msg);
    
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
    game.game_thread.join();
    delete[] game.opponent_addr;
    std::memset(&game, 0, sizeof(game));
    flush_buffer(GAME_BUFFER, current_game_index);
}

void Network::announce_status(char* addr, const char* status, char**& game_status) {
    
    char* local_addr = net_config.address;
    char** devices = net_config.devices;
    
    if (addr == nullptr) {
        addr = local_addr;
    }
    
    // addr::addr::status
    size_t msg_len = INET_ADDRSTRLEN + 2 + std::strlen(status);
    char* msg = new char[msg_len];
    std::memset(msg, 0, msg_len);

    std::snprintf(msg, msg_len, "%s::%s", addr, status);
    
    for (int i=0; i!=NUMBER_OF_DEVICES-1; i++) {
        char* dest_addr = devices[i];
        if (std::strncmp(addr, dest_addr, INET_ADDRSTRLEN) != 0) {
            send_message(chlg_sckt, dest_addr, CHLG_PORT, msg);
        }
    }
    
    delete[] msg;
    
    for (int i=0; i!=16; i++) {
        if (*game_status[i] == '\0') {
            size_t own_msg_len = INET_ADDRSTRLEN + 2 + INET_ADDRSTRLEN + 2 + std::strlen(status);
            char* own_msg = new char[own_msg_len];
            std::snprintf(own_msg, own_msg_len, "%s::%s::%s", local_addr, addr, status);
            std::memcpy(game_status[i], own_msg, own_msg_len);
            delete[] own_msg;
            break;
        }
    }
}

void Network::announce_master() {
    
    char** devices = net_config.devices;
    
    for (int i=0; i!=NUMBER_OF_DEVICES-1; i++) {
        char* addr = devices[i];
        send_message(chlg_sckt, addr, CHLG_PORT, "MASTER");
    }
    
    std::cout << "Master: " << net_config.address << std::endl;
}

void Network::listen_for_master(char*& addr) {
    
    bool master_announced = false;
    
    while (!master_announced) {
        for (int i=0; i!=MSG_BUFFER_SIZE; i++) {
            if (*CHLG_BUFFER[i] == '\0') {
                break;
            }
            
            char* buffer_msg = new char[MESSAGE_SIZE];
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                std::memcpy(buffer_msg, CHLG_BUFFER[i], MESSAGE_SIZE);
            }
            
            char* recv_addr;
            char* msg;
            
            split_buffer_message(recv_addr, msg, buffer_msg);
            
            delete[] buffer_msg;
            
            if (std::strncmp(msg, "MASTER", 6) == 0) {
                master_announced = true;
                addr = new char[INET_ADDRSTRLEN];
                std::memcpy(addr, recv_addr, INET_ADDRSTRLEN);
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Network::start_ntp_server(uint32_t& start_time) {
    
    ntp_thread_active = true;
    ntp_thread = std::thread(&Network::ntp_server, this, std::ref(start_time));
}

void Network::stop_ntp_server() {
    close(ntp_sckt);
    ntp_thread.join();
}

void Network::ntp_server(uint32_t& start_time) {
    
    while (ntp_thread_active) {
        
        NTPPacket packet;
        std::memset(&packet, 0, NTP_PACKET_SIZE);
        
        struct sockaddr_in src_addr;
        std::memset(&src_addr, 0, sizeof(src_addr));
        socklen_t src_addr_len = sizeof(src_addr);
        
        if (recvfrom(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error Receiving NTP Request: " << std::strerror(errno) << std::endl;
            }
            continue;
        }
        
        if (!packet.time_request) {
            packet.req_recv_time = htonl((uint32_t) time(NULL) + 2208988800UL);
            packet.res_trans_time = htonl((uint32_t) time(NULL) + 2208988800UL);
            
            if (sendto(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &src_addr, src_addr_len) < 0) {
                std::cerr << "Failed Sending NTP Response≤." << std::endl;
            }
        } else {
            if (start_time == 0) {
                start_time = (uint32_t) time(NULL) + 2UL;
                std::cout << "Start Time Determined..." << std::endl;
            }
            
            packet.start_time = htonl(start_time);
            
            if (sendto(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &src_addr, src_addr_len) < 0) {
                std::cerr << "Error sending start time: " << std::strerror(errno) << std::endl;
            }
        }
    }
}

    
NTPPacket Network::ntp_listener(bool* received=nullptr, bool time_request=false) {
    NTPPacket packet;
    
    std::memset(&packet, 0, NTP_PACKET_SIZE);
    
    sockaddr_in src_addr;
    std::memset(&src_addr, 0, sizeof(src_addr));
    socklen_t src_addr_len = sizeof(src_addr);
    
    while (received == nullptr || !(*received)) {
        
        if (recvfrom(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len) < 0) {
            if (errno != 0x23 && errno != 0xB) {
                std::cerr << "Error Receiving NTP Response: " << std::strerror(errno) << std::endl;
            }
            continue;
        }
        
        if (time_request) {
            if (!packet.time_request) {
                continue;
            }
        }
        
        packet.res_recv_time = htonl((uint32_t) time(NULL) + 2208988800UL);
        
        if (received == nullptr) {
            received = new bool;
        }
        *received = true;
    }

    return packet;
}

NTPPacket Network::request_time(char* addr) {
    
    NTPPacket packet;
    std::memset(&packet, 0, sizeof(packet));
    
    bool server_available = false;
    
    std::thread recv_thread([this, &packet, &server_available]() {
        packet = ntp_listener(&server_available);
    });
    
    while (!server_available) {
        
        struct sockaddr_in dest_addr;
        std::memset(&dest_addr, 0, sizeof(dest_addr));
        
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(NTP_PORT);
        dest_addr.sin_addr.s_addr = inet_addr(addr);
        
        packet.req_trans_time = htonl((uint32_t) time(NULL) + 2208988800UL);
        if (sendto(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
            std::cerr << "Error Requesting NTP: " << std::strerror(errno) << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
    }
    recv_thread.join();
    
    packet.req_trans_time = ntohl(packet.req_trans_time);
    packet.req_recv_time = ntohl(packet.req_recv_time);
    packet.res_trans_time = ntohl(packet.res_trans_time);
    packet.res_recv_time = ntohl(packet.res_recv_time);
    
    return packet;
}

uint32_t Network::request_start_time(char* addr) {
    
    while (true) {

        NTPPacket packet;
        std::memset(&packet, 0, NTP_PACKET_SIZE);
        
        packet.time_request = true;
        
        sockaddr_in dest_addr;
        std::memset(&dest_addr, 0, sizeof(dest_addr));
        
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(NTP_PORT);
        dest_addr.sin_addr.s_addr = inet_addr(addr);
        
        if (sendto(ntp_sckt, &packet, NTP_PACKET_SIZE, 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
            std::cerr << "Error sending start time: " << std::strerror(errno) << std::endl;
            continue;
        }
        
        packet = ntp_listener(nullptr, true);
        
        return ntohl(packet.start_time);
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

int Network::get_buffer_size() {
    return MSG_BUFFER_SIZE;
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
