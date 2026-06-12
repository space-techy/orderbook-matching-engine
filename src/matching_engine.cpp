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

    OrderResults MatchingEngine::make_reject(ClientOrderId client_order_id, ClientId client_id,
                                              Symbol symbol, Side side, Ticks price,
                                              Qty qty, std::string error_text) {
        OrderResults r;
        r.message_code = MessageCode::OrderRejected;
        r.error        = std::move(error_text);
        // order_id 0 — the request never made it into the book, so no internal
        // id exists. The row names the order by the client's handle for it.
        r.orders.push_back({0, client_order_id, client_id, symbol, side, price, qty, qty,
                            MessageCode::OrderRejected});
        return r;
    }

    OrderResults MatchingEngine::process_order(const Message& msg, ConnId conn_id) {
        // For a new_order the request's ticket IS the order's identity; for
        // cancel/modify the targeted order is named separately — reject rows
        // should describe the TARGET, not the request.
        const ClientOrderId affected_client_order_id =
            (msg.type == MessageType::NewOrder) ? msg.client_order_id
                                                : msg.target_client_order_id;

        if (msg.type == MessageType::NewOrder || msg.type == MessageType::Modify) {
            if (msg.qty == 0) {
                return make_reject(affected_client_order_id, msg.client_id, msg.symbol, msg.side,
                                   msg.price, msg.qty, "qty must be > 0");
            }
        }
        if (msg.type == MessageType::NewOrder) {
            if (msg.price <= 0) {
                return make_reject(affected_client_order_id, msg.client_id, msg.symbol, msg.side,
                                   msg.price, msg.qty, "price must be > 0");
            }
        }

        OrderBook& book = symbol_table_[msg.symbol];

        switch (msg.type) {
            case MessageType::NewOrder: {
                // Birth of the internal id: the engine names the order, the
                // client's ticket rides along inside it for outbound stamping.
                Order o{
                    msg.client_id, next_order_id_++, msg.client_order_id,
                    msg.symbol, msg.side, msg.qty, msg.qty, msg.price,
                    msg.order_type, conn_id
                };
                return book.process_new_order(std::move(o), next_trade_id_);
            }
            case MessageType::Cancel:
                return book.cancel_order(msg.target_client_order_id, msg.client_id);

            case MessageType::Modify:
                return book.modify_order(msg.target_client_order_id, msg.client_id, msg.qty);
        }

        return make_reject(affected_client_order_id, msg.client_id, msg.symbol, msg.side,
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
                // Direct response: echo the request's ticket. This is the
                // bot's pairing key — pending.pop(client_order_id).
                output_queue_->push_order(OutputMessage{
                    item->conn_id, std::move(r), seq,
                    item->message.client_order_id, std::nullopt});

                // Unsolicited notices: NO request ticket (nobody asked — that
                // absence is the signal), orig says which order it is about.
                for (auto& ev : resting_events) {
                    const ClientOrderId original_client_order_id = ev.order.client_order_id;
                    OrderResults sub;
                    sub.message_code = ev.order.message_code;
                    sub.orders.push_back(std::move(ev.order));
                    sub.trades.push_back(std::move(ev.trade));
                    output_queue_->push_order(OutputMessage{
                        ev.conn_id, std::move(sub), seq,
                        std::nullopt, original_client_order_id});
                }
            }
        }
    }

}
