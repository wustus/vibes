//
//  SynchronizationHandler.cpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#include "synchronization_handler.hpp"


SynchronizationHandler::SynchronizationHandler(Network& net) : network(net) {

    is_master = false;
    ntp_server = nullptr;
    start_time = 0;
    offset = 0;
    
    game_status = new char*[16];
    
    for (int i=0; i!=16; i++) {
        game_status[i] = new char[128];
        game_status[i][0] = '\0';
    }
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
        return;
    } else {
        std::cout << " | Lost.           |" << std::endl;
        std::cout << " -------------------" << std::endl;
    }
    
    if (ttt.is_won) {
        network.announce_result(challenger, "WIN", game_status);
    } else {
        network.announce_result(challenger, "LOSE", game_status);
    }
}

void SynchronizationHandler::reset_game() {
    memset(&ttt, 0, sizeof(ttt));
    for (int i=0; i!=9; i++) {
        ttt.field[i] = ' ';
    }
}

void SynchronizationHandler::determine_master() {
    
    bool next_round = true;

    network.start_challenge_listener();
    std::thread status_listener([this, &next_round]() { network.game_status_listener(game_status, next_round); });
    
    while (next_round) {
        char* challenger = network.find_challenger(game_status);
        
        std::cout << "Found Challenger: " << challenger << std::endl;
        
        play(challenger);
        delete[] challenger;
        
        next_round = ttt.is_won;
        
        if (next_round) {
            int c = 0;
            for (int i=0; i!=16; i++) {
                if (*game_status[i] != '\0') {
                    c++; 
                    continue;
                }
                break;
            }
            if (c == 1) {
                is_master = true;
                next_round = false;
                break;
            }
            network.flush_game_buffer();
        }
    }
    
    /*
     TODO: fix not received move
     */
    
    status_listener.join();
    
    if (is_master) {
        network.announce_master();
        std::cout << "Starting NTP Server" << std::endl;
        network.start_ntp_server(start_time);
    } else {
        std::cout << "Wait Until NTP-Master is Determined..." << std::endl;
        network.listen_for_master(ntp_server);
        std::cout << "NTP-Master: " << ntp_server << std::endl;
    }
}

bool SynchronizationHandler::get_is_master() {
    return is_master;
}

void SynchronizationHandler::set_offset() {

    if (!is_master) {
        uint32_t offset = 0;
        
        for (int i=0; i!=10; i++) {
            NTPPacket packet;
            std::memset(&packet, 0, NTP_PACKET_SIZE);
            
            packet.time_request = false;
            packet = network.request_time(ntp_server);
            
            uint64_t temp_offset = ((packet.req_recv_time - packet.req_trans_time) + (packet.res_recv_time - packet.res_trans_time)) / 2;
            offset += temp_offset;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        offset /= 10;
        
        std::cout << "Average Offset: " << offset << std::endl;
        
        this->offset = offset;
    }
}

uint32_t SynchronizationHandler::get_offset() {
    return offset;
}

uint32_t SynchronizationHandler::get_start_time() {
    
    if (is_master) {
        while (start_time == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return start_time;
    }
    
    return network.request_start_time(ntp_server);
}
