#pragma once

#include "matching-engine/order.hpp"

#include<list>
#include<map>
#include<utility>
#include<vector>


namespace me::matching{
    
    class OrderBook{
        public:
            std::pair<bool, MessageCode> add_order(Order& order);
            std::pair<bool, MessageCode> cancel_order(OrderId order_id, ClientId client_id);
            std::pair<bool, MessageCode> modify_order(OrderId order_id, ClientId cliend_id, Order& order);

            std::pair<std::pair<std::vector<Trade>, std::vector<Order>>, MessageCode> match(Order& incoming_order);

            std::map<Ticks, std::list<Order>, std::greater<>>::iterator best_bid();
            std::map<Ticks, std::list<Order>, std::less<>>::iterator best_ask();   

        private:
            std::map<Ticks, std::list<Order>, std::greater<>> buys_;
            std::map<Ticks, std::list<Order>, std::less<>> sells_;
            std::map<OrderId, OrderLocation> reference_order;

    };

}