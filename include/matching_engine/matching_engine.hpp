#pragma once

#include "matching_engine/order_book.hpp"
#include<cstdint>
#include<chrono>
#include<list>
#include<map>
#include<utility>
#include<vector>

namespace me::matching{

    class MatchingEngine{
        public:
            OrderResults process_order(Message& order_message);
            Order make_order_from_message(Message& order_message);
        
        private:
            std::map<Symbol, OrderBook> SymbolTable;
        
    };

}