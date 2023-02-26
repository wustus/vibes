//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"

SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {
    
}

void SynchronizationHandler::determine_master() {
    
    int CHLG_PORT = network.get_chlg_port();
    int sckt = network.get_chlg_sckt();
    
    int NUMBER_OF_DEVICES = sizeof(network.get_network_config()->devices) / sizeof(char*);
    
    char** msg_buffer = new char*[NUMBER_OF_DEVICES];
    char** devc_buffer = new char*[NUMBER_OF_DEVICES];
    
    bool receiving = true;
    
    std::thread recv_thread([this, sckt, &msg_buffer, &devc_buffer, &receiving]() { network.receive_message(sckt, msg_buffer, devc_buffer, &receiving); });
    
    bool pending_challenge = false;
    bool challenged = false;
    bool challenger_found = false;
    bool wait_for_challenge = false;
    
    int device_index = 0;
    char* chlg_device;
    
    std::cout << "Finding Challenger for " << network.get_network_config()->address << std::endl;
    
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
            char* device_addr = network.get_network_config()->devices[device_index];
            network.send_message(sckt, device_addr, "CHLG", CHLG_PORT);
            pending_challenge = true;
            chlg_device = device_addr;
            std::cout << "Challenging " << chlg_device << std::endl;
        } else if (pending_challenge) {
            for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
                if (devc_buffer[i] != nullptr && std::string(devc_buffer[i]) == std::string(chlg_device)) {
                    if (std::string(msg_buffer[i]) == std::string("ACC")) {
                        std::cout << "Challenger found " << chlg_device << std::endl;
                        challenger_found = true;
                    } else if (std::string(msg_buffer[i]) == std::string("DEC")) {
                        if (device_index == NUMBER_OF_DEVICES-1) {
                            std::cout << "No Challenger found, waiting for challenge..." << std::endl;
                            wait_for_challenge = true;
                        } else {
                            std::cout << "Challenge declined by " << chlg_device << "..." << std::endl;
                            pending_challenge = false;
                            device_index++;
                        }
                    }
                }
            }
        } else if (challenged) {
            network.send_message(sckt, chlg_device, "ACC", CHLG_PORT);
            challenger_found = true;
            std::cout << "Accepting Challenge by " << chlg_device << std::endl;
            for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
                if (std::string(msg_buffer[i]) == std::string("CHLG") && std::string(devc_buffer[i]) != std::string(chlg_device)) {
                    network.send_message(sckt, devc_buffer[i], "DEC", CHLG_PORT);
                    std::cout << "Declining Challenge " << chlg_device << std::endl;
                }
            }
        }
    }
    
    std::cout << "Done Searching." << network.get_network_config()->address << std::endl;
    std::cout << (challenger_found ? "Starting Game." : "Waiting...") << std::endl;
    
    // accept first one, decline rest
    
    
}
