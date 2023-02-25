//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"

SynchronizationHandler::SynchronizationHandler() {
    
}

void SynchronizationHandler::determine_master() {
    
    int sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    int opt = 1;
    
    if (setsockopt(sckt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error while setting socket option: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    struct timeval timeout;
    std::memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    if (setsockopt(sckt, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error while setting timeout:" << std::strerror(errno) << std::endl;
        exit(1);
    }
    
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(123);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sckt, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) < 0) {
        std::cerr << "Error while binding address: " << std::strerror(errno) << std::endl;
        return(1);
    }
    
    int NUMBER_OF_DEVICES = sizeof(net_config.devices) / sizeof(char*);
    
    char** msg_buffer = new char*[NUMBER_OF_DEVICES];
    char** devc_buffer = new char*[NUMBER_OF_DEVICES];
    
    bool receiving = true;
    
    std::thread recv_thread(receive_message, sckt, std::ref(msg_buffer), std::ref(devc_buffer), &receiving);
    
    bool pending_challenge = false;
    bool challenged = false;
    bool challenger_found = false;
    bool wait_for_challenge = false;
    
    int device_index = 0;
    char* chlg_device;
    
    std::cout << "Finding Challenger for " << net_config.address << std::endl;
    
    // find challenger
    while (!challenger_found && !wait_for_challenge) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
        
        if (msg_buffer[0] != nullptr) {
            if (std::strcmp(msg_buffer[0], "CHLG") == 0) {
                challenged = true;
                chlg_device = devc_buffer[0];
                std::cout << "Challenged by " << chlg_device << std::endl;
            }
        }
        
        if (!pending_challenge && !challenged) {
            char* device_addr = net_config.devices[device_index];
            send_message(sckt, device_addr, "CHLG", 123);
            pending_challenge = true;
            chlg_device = device_addr;
            std::cout << "Challenging " << chlg_device << std::endl;
        } else if (pending_challenge) {
            for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
                if (std::string(devc_buffer[i]) == std::string(chlg_device)) {
                    if (std::string(msg_buffer[i]) == std::string("ACC")) {
                        std::cout << "Challenger found " << chlg_device << std::endl;
                        challenger_found = true;
                    } else if (std::string(msg_buffer[i]) == std::string("DEC")) {
                        if (device_index == NUMBER_OF_DEVICES-1) {
                            std::cout << "No Challenger found, waiting for challenge..." << std::endl;
                            wait_for_challenge = true;
                        } else {
                            device_index++;
                        }
                    }
                }
            }
        } else if (challenged) {
            send_message(sckt, chlg_device, "ACC", 123);
            challenger_found = true;
            std::cout << "Accepting Challenge by " << chlg_device << std::endl;
            for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
                if (std::string(msg_buffer[i]) == std::string("CHLG") && std::string(devc_buffer[i]) != std::string(chlg_device)) {
                    send_message(sckt, devc_buffer[i], "DEC", 123);
                    std::cout << "Declining Challenge " << chlg_device << std::endl;
                }
            }
        }
    }
    
    std::cout << "Done Searching." << net_config.address << std::endl;
    std::cout << (challenger_found ? "Starting Game." : "Waiting...") << std::endl;
    
    // accept first one, decline rest
    
    
}
