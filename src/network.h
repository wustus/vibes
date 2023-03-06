//
//  network.h
//  vibes
//
//  Created by Justus Stahlhut on 22.02.23.
//

#ifndef network_h
#define network_h

#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <string>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>

class Network {
private:
    struct NetworkConfig {
        char address[INET_ADDRSTRLEN];
        char** devices;
    };
    
    struct Game {
        std::thread game_thread;
        char* opponent_addr;
        bool is_game_live;
        short played_moves[9];
    };
    
    const char* ROUTER_ADDR = "192.168.2.1";
    const char* SSDP_ADDR = "239.255.255.250";
    const int SSDP_PORT = 1900;
    const int ACK_PORT = 1901;
    const int CHLG_PORT = 6967;
    const int NTP_PORT = 123;
    
    int ssdp_sckt;
    int ack_sckt;
    int chlg_sckt;
    int ntp_sckt;
    
    int NUMBER_OF_DEVICES;
    const int BUFFER_SIZE = 128;
    const int MESSAGE_SIZE = 512;
    
    char** RECEIVING_BUFFER;
    char** ACK_BUFFER;
    
    int current_index = 0;
    int current_ack_index = 0;
    
    std::mutex buffer_mutex;

    NetworkConfig net_config;
    
    int create_udp_socket(int);
    void split_buffer_message(char*& addr, char*& msg, char* buffer_msg);
    void append_to_buffer(char* addr, char* message, char**& buffer, int& counter);
    uint16_t checksum(char* data);
    void ack_listener();
    bool listen_for_ack(const char* addr, char* msg);
    void send_ack(const char* addr, const char* msg);
    bool send_message(int sckt, const char* addr, int port, const char* msg, short timeout);
    void receive_messages(int sckt, bool& receiving);

    bool challenge_handler(char*& challenger, bool& found_challenger);
    void game_status_listener(char**& game_status, bool& listening);
    void listen_for_ready(char* addr, bool& is_opponent_ready);
public:
    Network(int);
    ~Network();
    
    void set_local_addr();
    void discover_devices();
    char* find_challenger(char** game_status);
    void wait_until_ready(char* addr);
    void start_game(char* addr);
    short receive_move();
    void make_move(short m);
    void end_game();
    
    /*
     dont write to buffers from methods
      -> write methods that run synchronously and expect a result
     
     internal methods should be
        send_message(sckt, port, msg);
            -> create thread which listens on buffer to detect positive ack, if not detected, message is sent again
        receive_message(sckt, &buffer); // buffer is char** of size 1024 and works as a sliding window
            -> send ack
        bool listen_for_ack(devc) // listens for device::ACK
     
     external methods
        set_local_addr();
        discover_devices();
        char* find_challenger();
        send_move();
     
    */
    
    int get_ssdp_port();
    int get_ssdp_sckt();
    int get_ack_port();
    int get_ack_sckt();
    int get_chlg_port();
    int get_chlg_sckt();
    int get_ntp_port();
    int get_ntp_sckt();
    int get_number_of_devices();
    int get_buffer_size();
    int get_message_size();
    char** get_receiving_buffer();
    int get_current_index();
    NetworkConfig* get_network_config();
    Game game;
};



#endif /* network_h */
