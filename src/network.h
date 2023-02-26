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
    
    const char* SSDP_ADDR = "239.255.255.250";
    const int SSDP_PORT = 1900;
    const int ACK_PORT = 1901;
    const int CHLG_PORT = 8883;
    const int NTP_PORT = 123;
    
    int ssdp_sckt;
    int ack_sckt;
    int chlg_sckt;
    int ntp_sckt;

    NetworkConfig net_config;
public:
    Network();
    ~Network();
    int create_socket(int);
    void set_local_addr();
    void send_ssdp_message(int sckt, const char* msg);
    void send_ack_message(int sckt, in_addr_t addr, const char* msg);
    void receive_ssdp_message(int sckt, std::vector<char*>& devices, bool& discovering);
    void receive_ack_message(int sckt, std::vector<char*>& pending_devices, std::vector<char*>& devices, bool& acknowledging);
    void send_message(int sckt, char* addr, const char* msg, int port);
    void receive_message(int sckt, char**& buffer, char**& addr, bool* receiving);
    void discover_devices(int n_devices);
    int get_ssdp_sckt();
    int get_ack_sckt();
    int get_chlg_sckt();
    int get_ntp_sckt();
    NetworkConfig* get_network_config();
};



#endif /* network_h */
