#pragma once

#include <cstdint>

namespace me::matching {

    using OrderId       = uint64_t;   // engine-internal book id — never chosen by clients
    using ClientOrderId = uint64_t;   // client's per-REQUEST ticket (FIX calls this ClOrdID)
    using ClientId      = uint64_t;
    using Qty       = uint64_t;
    using Ticks     = int64_t;   // signed: lets price-validation reject negatives
    using Symbol    = uint64_t;
    using ConnId    = uint64_t;
    using TradeId   = uint64_t;
    using SeqNum    = uint64_t;

    enum class Side : uint8_t {
        Buy,
        Sell
    };

    enum class OrderType : uint8_t {
        Limit,
        Market,
        Ioc
    };

    enum class MessageType : uint8_t {
        NewOrder,
        Cancel,
        Modify
    };

    enum class MessageCode : uint8_t {
        OrderFilled    = 0,
        OrderResting   = 1,
        OrderCancelled = 2,
        OrderModified  = 3,
        PartialFill    = 4,
        OrderRejected  = 5,
    };

}
