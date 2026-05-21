#pragma once

#include<cstdint>
#include<chrono>

namespace me::matching{
    
    using OrderId = uint64_t;
    using ClientId = uint64_t;
    using Quantity = uint64_t;
    using Ticks = uint64_t;
    using Symbol = uint64_t;
    using Timestamp = std::chrono::system_clock::time_point;

    using TradeId = uint64_t;

    enum class Status : uint64_t {
        INPROGRESS,
        Rejected,
        Completed
    };

    enum class MessageCode : uint64_t {
        // Order completed
        OC,

        // Order resting
        REST,

        // New orders
        // This are for successful orders
        FO,  // First Order
        FFO, // Fully Filled order
        PFO, // Partiall Filled Order
        
        // rejected
        SO,  // self order, that is same client cannot have best ask and best bid

        // Modify Orders
        // Rejected
        SC,  // Side change from buy to sell or sell to buy
        QI,  // Tried Quantity Increase
        QZ,  // Quantity Zero Now allowed
        
        // Cancelled & Modify Orders
        // Rejected
        ONF, // Order Not found
    };

    enum class Side : uint16_t {
        BUY,
        SELL
    };

    enum class OrderType : uint16_t {
        LIMIT,
        MARKET,
        IOC
    };

    enum class MessageType : uint16_t {
        NEW_ORDER,
        CANCEL,
        MODIFY
    };

}