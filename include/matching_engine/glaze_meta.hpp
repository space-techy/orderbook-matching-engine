#pragma once

#include "matching_engine/order.hpp"
#include <glaze/glaze.hpp>
#include <glaze/core/meta.hpp>
#include <string>
#include <vector>

namespace me::matching {

    // ─────────────────────────────────────────────────────────────
    // Wire-format envelopes. Glaze meta is compile-time and one
    // meta per C++ type — envelope structs let us shape outbound JSON
    // independently of the in-memory representation.
    // ─────────────────────────────────────────────────────────────

    struct OrderResponseEnvelope {
        std::string              type;
        SeqNum                   sequence_number;
        int                      message_code;
        std::vector<Trade>       trades;
        std::vector<OrderStatus> orders;
        std::string              error;       // empty unless rejected
    };

    struct TradeBroadcastEnvelope {
        std::string type = "trade_broadcast";
        Symbol      symbol;
        Ticks       price;
        Qty         qty;
        std::string aggressor_side;   // "buy" or "sell"
    };

    // Map message_code → type string for the OrderResponse envelope.
    inline std::string type_for_code(MessageCode c) {
        switch (c) {
            case MessageCode::OrderFilled:    return "order_filled";
            case MessageCode::OrderResting:   return "order_resting";
            case MessageCode::OrderCancelled: return "order_cancelled";
            case MessageCode::OrderModified:  return "order_modified";
            case MessageCode::PartialFill:    return "partial_fill";
            case MessageCode::OrderRejected:  return "order_rejected";
        }
        return "order_response";
    }

}

// ─────────────────────────────────────────────────────────────
// Glaze meta specializations — MUST be in global namespace.
// ─────────────────────────────────────────────────────────────

template<>
struct glz::meta<me::matching::MessageType> {
    using enum me::matching::MessageType;
    static constexpr auto value = glz::enumerate(
        "new_order", NewOrder,
        "cancel",    Cancel,
        "modify",    Modify
    );
};

template<>
struct glz::meta<me::matching::Side> {
    using enum me::matching::Side;
    static constexpr auto value = glz::enumerate(
        "buy",  Buy,
        "sell", Sell
    );
};

template<>
struct glz::meta<me::matching::OrderType> {
    using enum me::matching::OrderType;
    static constexpr auto value = glz::enumerate(
        "limit",  Limit,
        "market", Market,
        "ioc",    Ioc
    );
};

template<>
struct glz::meta<me::matching::Message> {
    using T = me::matching::Message;
    static constexpr auto value = glz::object(
        "action",     &T::type,
        "client_id",  &T::client_id,
        "order_id",   &T::order_id,
        "symbol",     &T::symbol,
        "side",       &T::side,
        "order_type", &T::order_type,
        "price",      &T::price,
        "qty",        &T::qty
    );
};

template<>
struct glz::meta<me::matching::Trade> {
    using T = me::matching::Trade;
    static constexpr auto value = glz::object(
        "buyer_order_id",   &T::buyer_order_id,
        "seller_order_id",  &T::seller_order_id,
        "buyer_client_id",  &T::buyer_client_id,
        "seller_client_id", &T::seller_client_id,
        "price",            &T::price,
        "qty",              &T::qty,
        "symbol",           &T::symbol
    );
};

template<>
struct glz::meta<me::matching::OrderStatus> {
    using T = me::matching::OrderStatus;
    static constexpr auto value = glz::object(
        "order_id",      &T::order_id,
        "client_id",     &T::client_id,
        "symbol",        &T::symbol,
        "side",          &T::side,
        "price",         &T::price,
        "qty",           &T::qty,
        "remaining_qty", &T::remaining_qty,
        "message_code",  &T::message_code   // emitted as integer
    );
};

template<>
struct glz::meta<me::matching::OrderResponseEnvelope> {
    using T = me::matching::OrderResponseEnvelope;
    static constexpr auto value = glz::object(
        "type",            &T::type,
        "sequence_number", &T::sequence_number,
        "message_code",    &T::message_code,
        "trades",          &T::trades,
        "orders",          &T::orders,
        "error",           &T::error
    );
};

template<>
struct glz::meta<me::matching::TradeBroadcastEnvelope> {
    using T = me::matching::TradeBroadcastEnvelope;
    static constexpr auto value = glz::object(
        "type",           &T::type,
        "symbol",         &T::symbol,
        "price",          &T::price,
        "qty",            &T::qty,
        "aggressor_side", &T::aggressor_side
    );
};
