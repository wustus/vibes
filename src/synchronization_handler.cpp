//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"


SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {}

void SynchronizationHandler::play(char* challenger) {
    
    std::cout << "Starting Game." << std::endl;
    network.start_game(challenger);
    reset_game();
    
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
            short move;
            while (ttt.is_move) {
                move = rand() % 9;
                ttt.make_move(move);
            }
            
            network.make_move(move);
            
        } else {
            short move = network.receive_move();
        }
    }
    
    network.end_game();
    
    if (ttt.is_won) {
        std::cout << "Won." << std::endl;
    } else if (ttt.is_draw) {
        std::cout << "Draw." << std::endl;
        play(challenger);
    } else {
        std::cout << "Lost." << std::endl;
    }
}

void SynchronizationHandler::reset_game() {
    memset(&ttt, 0, sizeof(ttt));
    for (int i=0; i!=9; i++) {
        ttt.field[i] = ' ';
    }
}

void SynchronizationHandler::determine_master() {
    
    char** game_status;
    
    char* challenger = network.find_challenger(game_status);
    std::cout << "Found Challenger: " << challenger << std::endl;
    
    play(challenger);
    
    
    delete[] challenger;
}

