#pragma once

#include<string>
#include "matching_engine/order.hpp"

namespace me::matching{
    std::string get_message_from_code(MessageCode m);
    void print_order(Order& o);
    void print_trade(const Trade& t);
} // namespace me::matching
