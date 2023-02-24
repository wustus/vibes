//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"

SynchronizationHandler::SynchronizationHandler(std::vector<char*> devices) {
    SynchronizationHandler::devices = devices;
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
    
    for (int i=0; i!=sizeof(net_config.devices) / sizeof(char*); i++) {
        char* devc = net_config.devices[i];
        send_message(sckt, devc, "CHLG", 123);
    }
    
    
}
