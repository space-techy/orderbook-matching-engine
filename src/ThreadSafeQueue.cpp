#include "include/ThreadSafeQueue.hpp"

#include<mutex>
#include<condition_variable>
#include <deque>
#include <optional>

namespace me::matching{
    template<typename T>
    void ThreadSafeQueue<T>::push_order(T item){
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(item));
        }
        cv_.notify_one();
    };

    template<typename T>
    std::optional<T> ThreadSafeQueue<T>::pop_order(){
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait( lock, [this]{ return !queue_.empty() || stopped; })
        if(stopped && queue_.empty()){
            return std::nullopt;
        }
        T order = std::move(queue_.front());
        queue_.pop_front();
        return order;
    }

    template<typename T>
    void ThreadSafeQueue<T>::stop(){
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped = true;
        }
        cv_.notify_all();
    }

    template<typename T>
    size_t ThreadSafeQueue<T>::size() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
}