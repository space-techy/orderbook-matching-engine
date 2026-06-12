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
            // Cancel/modify name their target by the client_order_id of the
            // new_order that created it (FIX OrigClOrdID) — translated to the
            // internal book id at this edge, never used past it.
            OrderResults cancel_order(ClientOrderId target_client_order_id, ClientId client_id);
            OrderResults modify_order(ClientOrderId target_client_order_id, ClientId client_id, Qty new_qty);

        private:
            OrderResults match_and_rest(Order incoming, std::atomic<TradeId>& next_trade_id);

            std::map<Ticks, std::list<Order>, std::greater<>> buys_;
            std::map<Ticks, std::list<Order>, std::less<>>    sells_;
            std::unordered_map<OrderId, OrderLocation>        reference_order_;

            // client's name for a live order → book's name. Entries live exactly
            // as long as the order does (inserted on rest, erased on terminal),
            // so it stays O(active orders), not O(all orders ever).
            std::unordered_map<ClientOrderId, OrderId>        client_order_id_to_internal_;
    };

}
