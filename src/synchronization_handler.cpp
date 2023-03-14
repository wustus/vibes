//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"


SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {
    SynchronizationHandler::ntp_server = nullptr;
}

void SynchronizationHandler::play(char* challenger) {
    
    reset_game();
    std::cout << "Waiting Until Opponent is Ready." << std::endl;
    network.wait_until_ready(challenger);
    
    std::cout << "Starting Game." << std::endl;
    network.start_game(challenger);
    
    if (std::strcmp(network.get_network_config()->address, challenger) < 0) {
        ttt.player = 'X';
        ttt.opponent = 'O';
        ttt.is_move = true;
    } else {
        ttt.player = 'O';
        ttt.opponent = 'X';
        ttt.is_move = false;
    }
    
    std::cout << " -------------------" << std::endl;
    std::cout << " | Player: " << ttt.player << "       |" << std::endl;
    std::cout << " | Opponent: " << ttt.opponent << "     |" << std::endl;
    std::cout << " -------------------" << std::endl;
    
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
            ttt.make_move(move);
        }
    }
    
    network.end_game();
    
    std::cout << " -------------------" << std::endl;
    
    if (ttt.is_won) {
        std::cout << " | Won.            |" << std::endl;
        std::cout << " -------------------" << std::endl;
    } else if (ttt.is_draw) {
        std::cout << " | Draw.           |" << std::endl;
        std::cout << " -------------------" << std::endl;
        play(challenger);
    } else {
        std::cout << " | Lost.           |" << std::endl;
        std::cout << " -------------------" << std::endl;
    }
    
    is_master = ttt.is_won;
    
    if (!is_master) {
        ntp_server = new char[INET_ADDRSTRLEN];
        std::memcpy(ntp_server, challenger, INET_ADDRSTRLEN);
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
    
    if (is_master) {
        network.start_ntp_server();
    }
}

uint64_t SynchronizationHandler::sync() {

    if (!is_master) {
        NTPPacket packet = network.request_time(ntp_server);
        
        uint64_t offset = ((packet.req_recv_time - packet.req_trans_time) + (packet.res_recv_time - packet.res_trans_time)) / 2;
        
        std::cout << "Offset: " << offset;
        
        return offset;
    }
    
    
    return 0;
}
