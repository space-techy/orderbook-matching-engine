#pragma once

#include "matching_engine/order.hpp"
#include <string>

namespace me::matching {

    std::string message_code_name(MessageCode m);

    // Debug printers (used by ME_LOG path).
    void print_order(const Order& o);
    void print_trade(const Trade& t);

}
