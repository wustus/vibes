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
#include <atomic>
#include <deque>
#include <condition_variable>
#include <functional>
#include <list>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>

struct Message {
    int sckt;
    char* addr;
    int port;
    char* msg;
    int timeout;
    
    Message() : sckt(0), addr(nullptr), port(0), msg(nullptr), timeout(0) {}
    
    Message(int sckt, char* addr, int port, char* msg, int timeout) : sckt(sckt), port(port), timeout(timeout) {
        
        this->addr = new char[INET_ADDRSTRLEN];
        std::memcpy(this->addr, addr, INET_ADDRSTRLEN);
        
        this->msg = new char[std::strlen(msg)+1];
        std::memcpy(this->msg, msg, std::strlen(msg));
    }
    
    Message(const Message& other) : sckt(other.sckt), port(other.port), timeout(other.timeout) {
        
        addr = new char[INET_ADDRSTRLEN];
        std::memcpy(addr, other.addr, INET_ADDRSTRLEN);
        
        msg = new char[std::strlen(other.msg)];
        std::memcpy(msg, other.msg, std::strlen(other.msg));
    }
    
    Message& operator=(Message& other) {
        
        if (this == &other) {
            return *this;
        }
        
        delete[] addr;
        delete[] msg;
        
        sckt = other.sckt;
        port = other.port;
        timeout = other.timeout;
        
        addr = new char[INET_ADDRSTRLEN];
        std::memcpy(addr, other.addr, INET_ADDRSTRLEN);
        
        msg = new char[std::strlen(other.msg)+1];
        std::memcpy(msg, other.msg, std::strlen(other.msg));
        
        return *this;
    }
    
    ~Message() {
        delete[] addr;
        delete[] msg;
    }
};

struct NTPPacket {
    bool time_request;
    char padding;
    uint32_t req_trans_time;
    uint32_t req_recv_time;
    uint32_t res_trans_time;
    uint32_t res_recv_time;
    uint32_t start_time;
};

class ThreadPool {
private:
    size_t number_of_threads;
    std::vector<std::thread> threads;
    std::list<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    bool stop = false;
public:
    ThreadPool(size_t number_of_threads) {
        ThreadPool::number_of_threads = number_of_threads;
        start();
    }
    
    ~ThreadPool() {
        stop_and_flush_threads();
    }
    
    void start() {
        
        if (!stop) {
            return;
        }
        
        stop = false;
        threads.clear();
        
        for (int i=0; i!=number_of_threads; i++) {
            threads.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        condition.wait(lock, [this] { return !tasks.empty() || stop; });
                        
                        if (stop) {
                            break;
                        }
                        
                        task = std::move(tasks.front());
                        tasks.pop_front();
                    }
                    task();
                }
            });
        }
    }
    
    void enqueue_task(std::function<void()> task) {
        if (is_running()) {
            {
                std::unique_lock<std::mutex> locK(mutex);
                tasks.push_back(task);
            }
            condition.notify_one();
        }
    }
    
    void stop_and_flush_threads() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stop = true;
        }
        
        condition.notify_all();
        for (std::thread& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        {
            std::unique_lock<std::mutex> lock(mutex);
            tasks.clear();
        }
    }
    
    bool is_running() {
        return !stop;
    }
};

class Network {
private:
    struct NetworkConfig {
        char* address;
        char** devices;
        size_t number_of_devices;
        
        NetworkConfig() : address(nullptr), devices(nullptr), number_of_devices(0) {}
        
        NetworkConfig(size_t number_of_devices) : number_of_devices(number_of_devices) {
            
            address = new char[INET_ADDRSTRLEN];
            devices = new char*[number_of_devices-1];
            
            for (int i=0; i!=number_of_devices-1; i++) {
                devices[i] = new char[INET_ADDRSTRLEN];
                *devices[i] = '\0';
            }
        }
        
        NetworkConfig(char* address, char** devices, size_t number_of_devices) {
            this->address = new char[INET_ADDRSTRLEN];
            std::memcpy(this->address, address, INET_ADDRSTRLEN);
            
            devices = new char*[number_of_devices-1];
            for (int i=0; i!=number_of_devices-1; i++) {
                devices[i] = new char[INET_ADDRSTRLEN];
                *devices[i] = '\0';
            }
            
            this->number_of_devices = number_of_devices;
        }
        
        NetworkConfig& operator=(NetworkConfig &other) {
            if (this == &other) {
                return *this;
            }
            
            delete[] address;
            for (int i=0; i!=number_of_devices-1; i++) {
                delete[] devices[i];
            }
            
            delete[] devices;
            
            std::memcpy(address, other.address, sizeof(INET_ADDRSTRLEN));
            
            devices = new char*[other.number_of_devices-1];
            for (int i=0; i!=other.number_of_devices-1; i++) {
                devices[i] = new char[INET_ADDRSTRLEN];
                *devices[i] = '\0';
            }
            
            number_of_devices = other.number_of_devices;
            
            return *this;
        }
        
        ~NetworkConfig() {
            delete[] address;
            
            for (int i=0; i!=number_of_devices-1; i++) {
                delete[] devices[i];
            }
            
            delete[] devices;
        }
        
        void set_address(char* address) {
            std::memcpy(this->address, address, INET_ADDRSTRLEN);
        }
        
        void add_device(char* device) {
            for (int i=0; i!=number_of_devices-1; i++) {
                if (*devices[i] == '\0') {
                    std::memcpy(devices[i], device, INET_ADDRSTRLEN);
                    break;
                }
            }
        }
    };
    
    struct Game {
        std::thread game_thread;
        char* opponent_addr;
        bool is_game_live;
        short played_moves[9];
    };
    
    #define NTP_PACKET_SIZE 24
    #define MSG_BUFFER_SIZE 128
    #define MESSAGE_SIZE 512
    
    const char* ROUTER_ADDR = "192.168.2.1";
    const char* SSDP_ADDR = "239.255.255.250";
    const int SSDP_PORT = 1900;
    const int ACK_PORT = 1901;
    const int DISC_PORT = 1902;
    const int CHLG_PORT = 1903;
    const int GAME_PORT = 1904;
    const int NTP_PORT = 123;
    
    int ssdp_sckt;
    int ack_sckt;
    int disc_sckt;
    int chlg_sckt;
    int game_sckt;
    int ntp_sckt;
    
    int NUMBER_OF_DEVICES;
    
    std::deque<Message> message_buffer;
    
    char** RECEIVING_BUFFER;
    char** ACK_BUFFER;
    char** CHLG_BUFFER;
    char** GAME_BUFFER;
    
    int current_index = 0;
    int current_ack_index = 0;
    int current_chlg_index = 0;
    int current_game_index = 0;
    
    std::mutex buffer_mutex;
    std::mutex sender_mutex;
    std::mutex var_mutex;

    NetworkConfig net_config;
    
    std::thread transmission_thread;
    std::thread ack_thread;
    std::thread chlg_thread;
    std::thread ntp_thread;
    
    bool transmission_thread_active = false;
    bool ack_thread_active = false;
    bool chlg_thread_active = false;
    bool ntp_thread_active = false;
    
    int create_udp_socket(int);
    void split_buffer_message(char*& addr, char*& msg, char* buffer_msg);
    void append_to_buffer(char* addr, char* message, char**& buffer, int& counter);
    void flush_buffer(char**& buffer, int& counter);
    uint16_t checksum(char* data);
    void ack_listener();
    void ack_handler(Message msg);
    bool listen_for_ack(char* addr, char* msg);
    void send_ack(char* addr, char* msg);
    void transmission_handler();
    void send_message(int sckt, const char* addr, int port, const char* msg, short timeout);
    void receive_messages(int sckt, bool& receiving, char**& buffer, int& counter);

    void listen_for_ready(char* addr, bool& is_opponent_ready);

    void ntp_server(uint32_t& start_time);
    NTPPacket ntp_listener(bool* received, bool time_request);
public:
    Network(int);
    ~Network();
    
    void set_local_addr();
    void discover_devices();
    void start_challenge_listener();
    char* find_challenger(char**& game_status);
    void wait_until_ready(char* addr);
    void start_game(char* addr);
    short receive_move();
    void make_move(short m);
    void end_game();
    void game_status_listener(char**& game_status, bool& listening);
    void announce_status(char* addr, const char* result, char**& game_status);
    void announce_master();
    void flush_game_buffer();
    
    void listen_for_master(char*& addr);
    void start_ntp_server(uint32_t& start_time);
    void stop_ntp_server();
    NTPPacket request_time(char* addr);
    
    void sync_handler(uint32_t& start_time);
    uint32_t request_start_time(char* addr);
    
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
    ThreadPool thread_pool;
};



#endif /* network_h */
