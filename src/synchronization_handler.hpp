//
//  SynchronizationHandler.hpp
//  vibes
//
//  Created by Justus Stahlhut on 24.02.23.
//

#ifndef SynchronizationHandler_hpp
#define SynchronizationHandler_hpp

#include "network.h"

struct tic_tac_toe {
    char field[9];
    bool is_move;
    bool is_over = false;
    bool is_won;
};

class SynchronizationHandler {
public:
    SynchronizationHandler();
    void determine_master();
    void sync();
private:
    bool is_master;
    uint64_t start_time;
};

#endif /* SynchronizationHandler_hpp */
