#pragma once

#include<mutex>
#include<condition_variable>
#include <deque>
#include <optional>

namespace me::matching{
    template<typename T>
    class ThreadSafeQueue{
        public:
            // We are just returning if we are able to succesfully add the order
            void push_order(T item);
            // We are trying to get all the order info by popping it so if it is emtpy or fails we pass something like null and if it is succesfull we pass the order back
            std::optional<T> pop_order();
            
            // To stop all waiting threads
            void stop();

            size_t size() const;

        private:
            std::deque<T> queue_;
            std::condition_variable cv_;
            std::mutex mutex_;
            bool stopped = false;
    };
}