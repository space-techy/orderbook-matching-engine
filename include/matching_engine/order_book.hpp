#pragma once

#include "matching_engine/order.hpp"

#include <atomic>
#include <list>
#include <map>
#include <unordered_map>

namespace me::matching {

    class OrderBook {
        public:
            OrderResults process_new_order(Order incoming, std::atomic<TradeId>& next_trade_id);
            OrderResults cancel_order(OrderId order_id, ClientId client_id);
            OrderResults modify_order(OrderId order_id, ClientId client_id, Qty new_qty);

        private:
            OrderResults match_and_rest(Order incoming, std::atomic<TradeId>& next_trade_id);

            std::map<Ticks, std::list<Order>, std::greater<>> buys_;
            std::map<Ticks, std::list<Order>, std::less<>>    sells_;
            std::unordered_map<OrderId, OrderLocation>        reference_order_;
    };

}
