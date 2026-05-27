#pragma once

#include "matching_engine/order.hpp"

#include<cstdint>
#include<chrono>
#include<list>
#include<map>
#include<utility>
#include<vector>


namespace me::matching{
    
    class OrderBook{
        public:
            std::pair<bool, MessageCode> add_order(Order& order);
            std::pair<bool, Order> cancel_order(OrderId order_id, ClientId client_id);
            std::pair<bool, Order> modify_order(OrderId order_id, ClientId cliend_id, Order& order);

            OrderResults match(Order& incoming_order);

            std::map<Ticks, std::list<Order>, std::greater<>>::iterator best_bid();
            std::map<Ticks, std::list<Order>, std::less<>>::iterator best_ask();   

        private:
            std::map<Ticks, std::list<Order>, std::greater<>> buys_;
            std::map<Ticks, std::list<Order>, std::less<>> sells_;
            std::map<OrderId, OrderLocation> reference_order;

    };

}