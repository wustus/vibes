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

struct NetworkConfig {
    char address[INET_ADDRSTRLEN];
    char** devices;
};

static const char* SSDP_ADDR = "239.255.255.250";
const int SSDP_PORT = 1900;
const int ACK_PORT = 1901;

NetworkConfig net_config;

void set_local_addr();
void send_ssdp_message(int sckt, const char* msg);
void send_ack_message(int sckt, in_addr_t addr, const char* msg);
void receive_ssdp_message(int sckt, std::vector<char*>& devices, bool& discovering);
void receive_ack_message(int sckt, std::vector<char*>& pending_devices, std::vector<char*>& devices, bool& acknowledging);
void send_message(int sckt, char* addr, const char* msg, int port);
char* receive_message(int sckt);
void discover_devices(int n_devices);


#endif /* network_h */
