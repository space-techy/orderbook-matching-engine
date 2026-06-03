#pragma once

#include "matching_engine/order.hpp"
#include <glaze/glaze.hpp>
#include <glaze/core/meta.hpp>

template<>
struct glz::meta<me::matching::MessageType> {
    using enum me::matching::MessageType;
    static constexpr auto value = glz::enumerate(
        "new_order", NEW_ORDER,
        "cancel", CANCEL,
        "modify", MODIFY
    );
};

template<>
struct glz::meta<me::matching::Side> {
    using enum me::matching::Side;
    static constexpr auto value = glz::enumerate(
        "buy", BUY,
        "sell", SELL
    );
};

template<>
struct glz::meta<me::matching::Message> {
    using T = me::matching::Message;
    static constexpr auto value = glz::object(
        "action", &T::type,
        "client_id", &T::client_id,
        "order_id", &T::order_id,
        "symbol", &T::symbol,
        "side", &T::side,
        "price", &T::price,
        "qty", &T::quantity,
        "type", &T::order_type
    );
};

template<>
struct glz::meta<me::matching::Trade> {
    using T = me::matching::Trade;
    static constexpr auto value = glz::object(
        "buyer_order_id", &T::buyer_order_id,
        "seller_order_id", &T::seller_order_id,
        "price", &T::price,
        "qty", &T::quantity,
        "symbol", &T::symbol
    );
};

template<>
struct glz::meta<me::matching::Order> {
    using T = me::matching::Order;
    static constexpr auto value = glz::object(
        "order_id", &T::order_id,
        "client_id", &T::client_id,
        "symbol", &T::symbol,
        "side", &T::side,
        "price", &T::price,
        "qty", &T::quantity,
        "remaining_qty", &T::remaining_quantity,
        "status", &T::status,
        "message_code", &T::msg_code
    );
};

template<>
struct glz::meta<me::matching::Error>{
    using T = me::matching::Error;
    static constexpr auto value = glz::object(
        "error", &T::error,
        "detail", &T::detail
    );
};
