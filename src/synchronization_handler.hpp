//
//  SynchronizationHandler.hpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#ifndef SynchronizationHandler_hpp
#define SynchronizationHandler_hpp

#include "network.h"

class SynchronizationHandler {
private:
    Network& network;
    
    struct tic_tac_toe {
        char field[9];
        char player;
        char opponent;
        bool is_move;
        bool is_over;
        bool is_draw;
        bool is_won;
        
        bool is_game_over() {
            for (int i=0; i!=9; i+=3) {
                if (field[i] != ' ' && field[i] == field[i+1] && field[i+1] == field[i+2]) {
                    is_won = field[i] == player;
                    return true;
                }
            }
            
            for (int i=0; i!=3; i++) {
                if (field[i] != ' ' && field[i] == field[i+3] && field[i+3] == field[i+6]) {
                    is_won = field[i] == player;
                    return true;
                }
            }
            
            if (field[0] != ' ' && field[0] == field[4] && field[4] == field[8]) {
                is_won = field[0] == player;
                return true;
            }
            
            if (field[0] != ' ' && field[0] == field[4] && field[4] == field[8]) {
                is_won = field[0] == player;
                return true;
            }
            
            if (field[2] != ' ' && field[2] == field[4] && field[4] == field[6]) {
                is_won = field[2] == player;
                return true;
            }
            
            for (int i=0; i!=9; i++) {
                if (field[i] == ' ') {
                    return false;
                }
            }
            
            is_draw = true;
            return true;
        }
        
        void print_field() {
            std::cout << " _ _ _ _ " << std::endl;
            for (int i=0; i!=9; i++) {
                if (i%3 == 0) {
                    std::cout << std::endl;
                }
                std::cout << " " << field[i] << " ";
            }
            std::cout << std::endl << " _ _ _ _ " << std::endl;
            std::cout << std::endl;
        }
        
        void make_move(short m) {
            if (field[m] == ' ') {
                field[m] = is_move ? player : opponent;
            } else {
                return;
            }
            
            print_field();
            
            is_move = !is_move;
        }
    };
    
    tic_tac_toe ttt;
    
    char** game_status;
    
    std::thread handler_thread;
    
    bool is_master;
    char* ntp_server;
    
    uint32_t start_time;
    uint32_t offset;
    
public:
    SynchronizationHandler(Network& network);
    void reset_game();
    void play(char*);
    void wait_for_challenge();
    void determine_master();
    bool get_is_master();
    void set_offset();
    uint32_t get_offset();
    uint32_t get_start_time();
    
};

#endif /* SynchronizationHandler_hpp */
