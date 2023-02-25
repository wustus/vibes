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
    Network network;
    bool is_master;
    uint64_t start_time;
    struct tic_tac_toe {
        char field[9];
        bool is_move;
        bool is_over = false;
        bool is_won;
    };
public:
    SynchronizationHandler(Network& network);
    void determine_master();
    void sync();
};

#endif /* SynchronizationHandler_hpp */
