#include "matching_engine/matching_engine.hpp"

#include <utility>

namespace me::matching {

    MatchingEngine::~MatchingEngine() {
        stop();
    }

    void MatchingEngine::submit(ConnId conn_id, Message msg) {
        input_queue_.push_order(QueueItem{conn_id, std::move(msg)});
    }

    void MatchingEngine::start() {
        running_ = true;
        worker_  = std::thread(&MatchingEngine::worker_loop, this);
    }

    void MatchingEngine::stop() {
        if (!running_.exchange(false)) return;
        input_queue_.stop();
        if (worker_.joinable()) worker_.join();
    }

    OrderResults MatchingEngine::make_reject(OrderId order_id, ClientId client_id,
                                              Symbol symbol, Side side, Ticks price,
                                              Qty qty, std::string error_text) {
        OrderResults r;
        r.message_code = MessageCode::OrderRejected;
        r.error        = std::move(error_text);
        r.orders.push_back({order_id, client_id, symbol, side, price, qty, qty,
                            MessageCode::OrderRejected});
        return r;
    }

    OrderResults MatchingEngine::process_order(const Message& msg, ConnId conn_id) {
        if (msg.type == MessageType::NewOrder || msg.type == MessageType::Modify) {
            if (msg.qty == 0) {
                return make_reject(msg.order_id, msg.client_id, msg.symbol, msg.side,
                                   msg.price, msg.qty, "qty must be > 0");
            }
        }
        if (msg.type == MessageType::NewOrder) {
            if (msg.price <= 0) {
                return make_reject(msg.order_id, msg.client_id, msg.symbol, msg.side,
                                   msg.price, msg.qty, "price must be > 0");
            }
        }

        OrderBook& book = symbol_table_[msg.symbol];

        switch (msg.type) {
            case MessageType::NewOrder: {
                Order o{
                    msg.client_id, msg.order_id, msg.symbol, msg.side,
                    msg.qty, msg.qty, msg.price, msg.order_type, conn_id
                };
                return book.process_new_order(std::move(o), next_trade_id_);
            }
            case MessageType::Cancel:
                return book.cancel_order(msg.order_id, msg.client_id);

            case MessageType::Modify:
                return book.modify_order(msg.order_id, msg.client_id, msg.qty);
        }

        return make_reject(msg.order_id, msg.client_id, msg.symbol, msg.side,
                           msg.price, msg.qty, "invalid action");
    }

    void MatchingEngine::worker_loop() {
        while (running_) {
            auto item = input_queue_.pop_order();
            if (!item.has_value()) break;

            OrderResults r = process_order(item->message, item->conn_id);
            const SeqNum seq = seq_.fetch_add(1) + 1;

            if (broadcast_queue_ && !r.trades.empty()) {
                BroadcastBatch batch;
                batch.reserve(r.trades.size());
                for (const auto& t : r.trades) {
                    batch.push_back(TradeBroadcast{t.symbol, t.price, t.qty, t.aggressor_side});
                }
                broadcast_queue_->push_order(std::move(batch));
            }

            auto resting_events = std::move(r.resting_events);
            r.resting_events.clear();

            if (output_queue_) {
                output_queue_->push_order(OutputMessage{item->conn_id, std::move(r), seq});

                for (auto& ev : resting_events) {
                    OrderResults sub;
                    sub.message_code = ev.order.message_code;
                    sub.orders.push_back(std::move(ev.order));
                    sub.trades.push_back(std::move(ev.trade));
                    output_queue_->push_order(OutputMessage{ev.conn_id, std::move(sub), seq});
                }
            }
        }
    }

}
