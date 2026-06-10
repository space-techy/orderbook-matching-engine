#pragma once

#include "matching_engine/order_book.hpp"
#include "ThreadSafeQueue.hpp"

#include <atomic>
#include <map>
#include <thread>
#include <vector>

namespace me::matching {

    using BroadcastBatch = std::vector<TradeBroadcast>;

    class MatchingEngine {
        public:
            MatchingEngine() = default;
            ~MatchingEngine();

            void submit(ConnId conn_id, Message msg);

            void start();
            void stop();

            void set_output_queue(ThreadSafeQueue<OutputMessage>* q)     { output_queue_ = q; }
            void set_broadcast_queue(ThreadSafeQueue<BroadcastBatch>* q) { broadcast_queue_ = q; }

        private:
            void worker_loop();

            OrderResults process_order(const Message& msg, ConnId conn_id);

            OrderResults make_reject(OrderId order_id, ClientId client_id,
                                     Symbol symbol, Side side, Ticks price,
                                     Qty qty, std::string error_text);

            std::map<Symbol, OrderBook> symbol_table_;
            ThreadSafeQueue<QueueItem>       input_queue_;
            ThreadSafeQueue<OutputMessage>*  output_queue_{nullptr};
            ThreadSafeQueue<BroadcastBatch>* broadcast_queue_{nullptr};

            std::thread       worker_;
            std::atomic<bool> running_{false};

            std::atomic<SeqNum>  seq_{0};
            std::atomic<TradeId> next_trade_id_{1};
    };

}
