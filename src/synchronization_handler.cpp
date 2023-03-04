//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"


SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {}

void SynchronizationHandler::play(char* challenger) {
    
    int sckt = network.get_chlg_sckt();
    int port = network.get_chlg_port();
    char* buffer = nullptr;
    bool receiving = true;
    bool ready = false;
    
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
            
            //network.send_message(sckt, challenger, std::to_string(move).c_str(), port);
        } else {
            
            if (!ready) {
              //  network.send_message(sckt, challenger, "READY", port);
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
    
        
    
}

