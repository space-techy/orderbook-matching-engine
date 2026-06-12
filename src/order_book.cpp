#include "matching_engine/order_book.hpp"

#include <algorithm>
#include <utility>

namespace me::matching {

    namespace {

        OrderStatus make_aggressor_status(const Order& o, MessageCode code) {
            return OrderStatus{
                o.order_id, o.client_order_id, o.client_id, o.symbol, o.side,
                o.price, o.qty, o.remaining_qty, code
            };
        }

        // Reject row for a cancel/modify whose target was never found in the
        // book — no internal id exists, so order_id is 0 and the row names the
        // order by the client's handle for it.
        OrderStatus make_not_found_status(ClientOrderId target_client_order_id, ClientId client_id) {
            return OrderStatus{
                0, target_client_order_id, client_id, 0, Side::Buy,
                0, 0, 0, MessageCode::OrderRejected
            };
        }

    }

    OrderResults OrderBook::cancel_order(ClientOrderId target_client_order_id, ClientId client_id) {
        OrderResults r;

        auto map_it = client_order_id_to_internal_.find(target_client_order_id);
        if (map_it == client_order_id_to_internal_.end()) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        auto it = reference_order_.find(map_it->second);
        if (it == reference_order_.end()) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }
        const auto& loc = it->second;
        if (loc.iter->client_id != client_id) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        Order snap = *loc.iter;
        if (loc.side == Side::Buy) {
            auto level = buys_.find(loc.price);
            level->second.erase(loc.iter);
            if (level->second.empty()) buys_.erase(level);
        } else {
            auto level = sells_.find(loc.price);
            level->second.erase(loc.iter);
            if (level->second.empty()) sells_.erase(level);
        }
        reference_order_.erase(it);
        client_order_id_to_internal_.erase(map_it);

        r.message_code = MessageCode::OrderCancelled;
        r.orders.push_back({snap.order_id, snap.client_order_id, snap.client_id,
                            snap.symbol, snap.side, snap.price, snap.qty, 0,
                            MessageCode::OrderCancelled});
        return r;
    }

    OrderResults OrderBook::modify_order(ClientOrderId target_client_order_id, ClientId client_id, Qty new_qty) {
        OrderResults r;

        if (new_qty == 0) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "qty must be > 0";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        auto map_it = client_order_id_to_internal_.find(target_client_order_id);
        if (map_it == client_order_id_to_internal_.end()) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        auto it = reference_order_.find(map_it->second);
        if (it == reference_order_.end()) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        auto& target = *it->second.iter;
        if (target.client_id != client_id) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "order not found";
            r.orders.push_back(make_not_found_status(target_client_order_id, client_id));
            return r;
        }

        if (new_qty >= target.remaining_qty) {
            r.message_code = MessageCode::OrderRejected;
            r.error = "qty increase or no-op not allowed";
            r.orders.push_back({target.order_id, target.client_order_id, target.client_id,
                                target.symbol, target.side, target.price, target.qty,
                                target.remaining_qty, MessageCode::OrderRejected});
            return r;
        }

        target.qty           = new_qty;
        target.remaining_qty = new_qty;

        r.message_code = MessageCode::OrderModified;
        r.orders.push_back({target.order_id, target.client_order_id, target.client_id,
                            target.symbol, target.side, target.price, target.qty,
                            target.remaining_qty, MessageCode::OrderModified});
        return r;
    }

    OrderResults OrderBook::process_new_order(Order incoming, std::atomic<TradeId>& next_trade_id) {
        return match_and_rest(std::move(incoming), next_trade_id);
    }

    OrderResults OrderBook::match_and_rest(Order incoming, std::atomic<TradeId>& next_trade_id) {
        OrderResults r;

        if (incoming.side == Side::Buy) {
            while (!sells_.empty() && incoming.remaining_qty > 0) {
                auto level_it = sells_.begin();
                if (level_it->first > incoming.price) break;

                auto& list = level_it->second;
                for (auto it = list.begin(); it != list.end() && incoming.remaining_qty > 0;) {
                    Order& resting = *it;
                    const Qty fill_qty = std::min(incoming.remaining_qty, resting.remaining_qty);

                    Trade t{};
                    t.trade_id               = next_trade_id.fetch_add(1);
                    t.buyer_order_id         = incoming.order_id;
                    t.seller_order_id        = resting.order_id;
                    t.buyer_client_order_id  = incoming.client_order_id;
                    t.seller_client_order_id = resting.client_order_id;
                    t.buyer_client_id        = incoming.client_id;
                    t.seller_client_id       = resting.client_id;
                    t.price                  = resting.price;
                    t.qty                    = fill_qty;
                    t.symbol                 = resting.symbol;
                    t.aggressor_side         = Side::Buy;
                    r.trades.push_back(t);

                    incoming.remaining_qty -= fill_qty;
                    resting.remaining_qty  -= fill_qty;

                    const bool resting_done = (resting.remaining_qty == 0);

                    RestingFill rf{};
                    rf.conn_id = resting.conn_id;
                    rf.order   = OrderStatus{
                        resting.order_id, resting.client_order_id, resting.client_id,
                        resting.symbol, resting.side, resting.price, resting.qty,
                        resting.remaining_qty,
                        resting_done ? MessageCode::OrderFilled : MessageCode::PartialFill
                    };
                    rf.trade = t;
                    r.resting_events.push_back(rf);

                    if (resting_done) {
                        client_order_id_to_internal_.erase(resting.client_order_id);
                        reference_order_.erase(resting.order_id);
                        it = list.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (level_it->second.empty()) sells_.erase(level_it);
            }
        } else {
            while (!buys_.empty() && incoming.remaining_qty > 0) {
                auto level_it = buys_.begin();
                if (level_it->first < incoming.price) break;

                auto& list = level_it->second;
                for (auto it = list.begin(); it != list.end() && incoming.remaining_qty > 0;) {
                    Order& resting = *it;
                    const Qty fill_qty = std::min(incoming.remaining_qty, resting.remaining_qty);

                    Trade t{};
                    t.trade_id               = next_trade_id.fetch_add(1);
                    t.buyer_order_id         = resting.order_id;
                    t.seller_order_id        = incoming.order_id;
                    t.buyer_client_order_id  = resting.client_order_id;
                    t.seller_client_order_id = incoming.client_order_id;
                    t.buyer_client_id        = resting.client_id;
                    t.seller_client_id       = incoming.client_id;
                    t.price                  = resting.price;
                    t.qty                    = fill_qty;
                    t.symbol                 = resting.symbol;
                    t.aggressor_side         = Side::Sell;
                    r.trades.push_back(t);

                    incoming.remaining_qty -= fill_qty;
                    resting.remaining_qty  -= fill_qty;

                    const bool resting_done = (resting.remaining_qty == 0);

                    RestingFill rf{};
                    rf.conn_id = resting.conn_id;
                    rf.order   = OrderStatus{
                        resting.order_id, resting.client_order_id, resting.client_id,
                        resting.symbol, resting.side, resting.price, resting.qty,
                        resting.remaining_qty,
                        resting_done ? MessageCode::OrderFilled : MessageCode::PartialFill
                    };
                    rf.trade = t;
                    r.resting_events.push_back(rf);

                    if (resting_done) {
                        client_order_id_to_internal_.erase(resting.client_order_id);
                        reference_order_.erase(resting.order_id);
                        it = list.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (level_it->second.empty()) buys_.erase(level_it);
            }
        }

        const bool any_trade = !r.trades.empty();
        if (incoming.remaining_qty == 0) {
            r.message_code = MessageCode::OrderFilled;
            r.orders.push_back(make_aggressor_status(incoming, MessageCode::OrderFilled));
            return r;
        }

        // Order rests — register it under BOTH names: the engine's internal id
        // (reference_order_) and the client's handle (client_order_id_to_internal_).
        if (incoming.side == Side::Buy) {
            buys_[incoming.price].push_back(incoming);
            auto it = std::prev(buys_[incoming.price].end());
            reference_order_[incoming.order_id] = {Side::Buy, incoming.price, it};
        } else {
            sells_[incoming.price].push_back(incoming);
            auto it = std::prev(sells_[incoming.price].end());
            reference_order_[incoming.order_id] = {Side::Sell, incoming.price, it};
        }
        client_order_id_to_internal_[incoming.client_order_id] = incoming.order_id;

        const MessageCode code = any_trade ? MessageCode::PartialFill : MessageCode::OrderResting;
        r.message_code = code;
        r.orders.push_back(make_aggressor_status(incoming, code));
        return r;
    }

}
