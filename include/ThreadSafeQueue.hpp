#pragma once

#include<mutex>
#include<condition_variable>
#include <deque>
#include <optional>

namespace me::matching{
    template<typename T>
    class ThreadSafeQueue{
        public:
            // Pushing order in threadsafe queue
            void push_order(T item){
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    queue_.push_back(std::move(item));
                }
                cv_.notify_one();
            };

            // We are trying to get all the order info by popping it so if it is emtpy or fails we pass something like null and if it is succesfull we pass the order back
            std::optional<T> pop_order(){
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait( lock, [this]{ return !queue_.empty() || stopped; });
                if(stopped && queue_.empty()){
                    return std::nullopt;
                }
                T order = std::move(queue_.front());
                queue_.pop_front();
                return order;
            }

            // To stop all waiting threads
            void stop(){
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stopped = true;
                }
                cv_.notify_all();
            }

            size_t size() const{
                std::lock_guard<std::mutex> lock(mutex_);
                return queue_.size();
            }
        private:
            std::deque<T> queue_;
            std::condition_variable cv_;
            std::mutex mutex_;
            bool stopped = false;
    };
}