#pragma once

#include "matching_engine/order_book.hpp"
#include "matching_engine/helper.hpp"
#include "ThreadSafeQueue.hpp"

#include<atomic>
#include<cstdint>
#include<chrono>
#include<list>
#include<map>
#include<string>
#include<thread>
#include<utility>
#include<vector>

namespace me::matching{
    struct QueueItem{
        ConnId conn_id;
        Message message;
    };

    struct ProcessedResult{
        ConnId conn_id;
        OrderResults order_results;
    };

    struct BroadcastMessage{
        MessageCode message_code;
        std::vector<Trade> trades;
    };

    class MatchingEngine{
        public:
            MatchingEngine() = default;
            ~MatchingEngine();

            // This takes order from w.s and adds in order queue
            void submit(ConnId& conn_id, Message& message);

            // Starting my matching engine on a thread
            void start();
            // Stopping my matching engine & Threads
            void stop();

            // Connect to output queues (owned by server, engine just points to them)
            void set_output_queue(ThreadSafeQueue<ProcessedResult>* queue);
            void set_broadcast_queue(ThreadSafeQueue<BroadcastMessage>* queue);
        
        private:
            // Internal Functions
            OrderResults process_order(Message& order_message);
            Order make_order_from_message(Message& order_message);

            // The single threaded loop
            void worker_loop();

            // Symbol Table
            std::map<Symbol, OrderBook> SymbolTable;

            // Input Queue initialized with matching engine
            ThreadSafeQueue<QueueItem> input_queue_;
            // Output Queue comes as pointer from outside of matching engine
            ThreadSafeQueue<ProcessedResult>* output_queue_ = nullptr;
            // Broadcasting Queue
            ThreadSafeQueue<BroadcastMessage>* broadcast_message_queue = nullptr;

            std::thread worker_;
            std::atomic<bool> running_{false};
        
    };

}