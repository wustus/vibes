//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"

SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {}

void SynchronizationHandler::play(char* challenger) {
    
    int sckt = network.get_game_sckt();
    int peer_sckt = -1;
    int port = network.get_game_port();
    char* buffer = nullptr;
    bool receiving = true;
    bool ready = false;
    
    std::thread accept_thread([this, sckt, &peer_sckt]() { peer_sckt = network.accept_connection(sckt); });
    
    network.connect_to_addr(sckt, challenger, port);
    
    while (peer_sckt == -1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
        
    std::thread recv_thread([this, sckt, &buffer, &receiving]() { network.receive_tcp_message(sckt, buffer, receiving); });
    
    network.send_tcp_message(sckt, "INIT");
    
     while (buffer == nullptr) {
        network.send_message(sckt, challenger, "INIT", port);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Starting Game." << std::endl;
    
    if (std::strcmp(network.get_network_config()->address, challenger) < 0) {
        ttt.player = 'X';
        ttt.opponent = 'O';
        ttt.is_move = true;
    } else {
        ttt.player = 'O';
        ttt.opponent = 'X';
        ttt.is_move = false;
    }
    
    while (!ttt.is_game_over()) {
        if (ttt.is_move) {
            
            while (!ready) {
                if (buffer && std::strcmp(buffer, "READY") == 0) {
                    ready = true;
                    buffer = nullptr;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            short move;
            while (ttt.is_move) {
                move = rand() % 9;
                ttt.make_move(move);
            }
            
            network.send_message(sckt, challenger, std::to_string(move).c_str(), port);
        } else {
            
            if (!ready) {
                network.send_message(sckt, challenger, "READY", port);
                ready = true;
                buffer = nullptr;
            }
            
            while (!ttt.is_move) {
                if (!buffer) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                short move = std::atoi(buffer);
                ttt.make_move(move);
                buffer = nullptr;
            }
        }
    }
    
    receiving = false;
    recv_thread.join();
    
    if (ttt.is_won) {
        std::cout << "Won." << std::endl;
    } else if (ttt.is_draw) {
        std::cout << "Draw." << std::endl;
        play(challenger);
    } else {
        std::cout << "Lost." << std::endl;
    }
    delete[] buffer;
}

void SynchronizationHandler::reset_game() {
    memset(&ttt, 0, sizeof(ttt));
    for (int i=0; i!=9; i++) {
        ttt.field[i] = ' ';
    }
}

void SynchronizationHandler::determine_master() {
    
    int CHLG_PORT = network.get_chlg_port();
    int sckt = network.get_chlg_sckt();
    
    int NUMBER_OF_DEVICES = network.get_number_of_devices();
    
    char** msg_buffer = new char*[NUMBER_OF_DEVICES]();
    char** devc_buffer = new char*[NUMBER_OF_DEVICES]();
    
    bool receiving = true;
    
    std::thread recv_thread([this, sckt, &msg_buffer, &devc_buffer, &receiving]() { network.receive_message(sckt, msg_buffer, devc_buffer, receiving); });
    
    bool pending_challenge = false;
    bool challenged = false;
    bool challenger_found = false;
    bool wait_for_challenge = false;
    
    int device_index = 0;
    char* chlg_device;
    
    std::cout << "Finding Challenger for " << network.get_network_config()->address << std::endl;
    // find challenger
    while (!challenger_found && !wait_for_challenge) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));
        
        if (msg_buffer[0] != nullptr) {
            if (std::string(msg_buffer[0]) == std::string("CHLG")) {
                challenged = true;
                chlg_device = devc_buffer[0];
                std::cout << "Challenged by " << chlg_device << std::endl;
            }
        }
        
        if (!pending_challenge && !challenged) {
            char* device_addr = network.get_network_config()->devices[device_index];
            std::cout << "Challenging " << device_addr << std::endl;
            network.send_message(sckt, device_addr, "CHLG", CHLG_PORT);
            pending_challenge = true;
            chlg_device = device_addr;
        } else if (pending_challenge && !challenged) {
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
                            std::cout << "Challenge declined by " << chlg_device << std::endl;
                            pending_challenge = false;
                            device_index++;
                        }
                    }
                }
            }
        } else if (challenged) {
            network.send_message(sckt, chlg_device, "ACC", CHLG_PORT);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            network.send_message(sckt, chlg_device, "ACC", CHLG_PORT);
            challenger_found = true;
            std::cout << "Accepting Challenge by " << chlg_device << std::endl;
            for (int i=0; i!=NUMBER_OF_DEVICES; i++) {
                if (msg_buffer[i] != nullptr && devc_buffer[i] != nullptr) {
                    if (std::string(msg_buffer[i]) == std::string("CHLG") && std::string(devc_buffer[i]) != std::string(chlg_device)) {
                        network.send_message(sckt, devc_buffer[i], "DEC", CHLG_PORT);
                        std::cout << "Declining Challenge " << chlg_device << std::endl;
                    }
                } else {
                    break;
                }
            }
        }
    }
    
    std::cout << "Device " << network.get_network_config()->address << " done searching." << std::endl;
    if (challenger_found) {
        std::cout << "Opponent: " << chlg_device << std::endl;
        receiving = false;
        recv_thread.join();
        reset_game();
        play(chlg_device);
    } else if (wait_for_challenge) {
        std::cout << "Waiting for challenger..." << std::endl;
    }
    
    if (ttt.is_won || wait_for_challenge) {
        
    } else {
        is_master = false;
        // sleep
    }
    
    
}
