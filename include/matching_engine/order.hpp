#pragma once

#include "matching_engine/type.hpp"
#include<list>
#include<cstdint>
#include<chrono>

namespace me::matching{
    struct OrderResults;
    struct Order;
    struct Trade;
    struct OrderLocation;
    struct Message;


    struct OrderResults{
        std::vector<Trade> Trades;
        std::vector<Order> Orders;
        MessageCode MessageCode;
    };


    struct Order{
        ClientId client_id;
        OrderId order_id;
        Symbol symbol;
        Side side;
        Quantity quantity;
        Quantity remaining_quantity; //We can think on like how to send back how much quantity is remaining without using this
        Ticks price;
        OrderType order_type;
        Timestamp arrival_time;
        Status status;
        MessageCode msg_code;
    };

    struct Trade{
        TradeId trade_id;
        OrderId buyer_order_id;
        OrderId seller_order_id;
        Ticks price;
        Quantity quantity;
        Symbol symbol;
        Timestamp timestamp;
    };

    struct OrderLocation{
        Side side;
        Ticks price;
        std::list<Order>::iterator iter;
    };
    
    struct Message{
        MessageType type;
        ClientId client_id;
        OrderId order_id;
        Symbol symbol;
        Timestamp arrival_time;

        // Modify Order
        Ticks price;
        Quantity quantity;

        // New order
        OrderType order_type;
        Side side;
    };

    struct Error{
        std::string error;
        std::string detail;
    };

}