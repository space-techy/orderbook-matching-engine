#include "matching_engine/helper.hpp"

#include<iostream>
#include<sstream>
#include<string>


namespace me::matching{
    std::string get_message_from_code(MessageCode m){
        switch (m)
        {
        case MessageCode::OC:
            return "Order Completed";
        case MessageCode::REST:
            return "Order Restinig";
        case MessageCode::FO:
            return "First Order";
        case MessageCode::FFO:
            return "Fully Filled Order";
        case MessageCode::PFO:
            return "Partially Filled Order";
        case MessageCode::SO:
            return "Self Order not allowed";
        case MessageCode::SC:
            return "Side change on order modification not allowed";
        case MessageCode::QI:
            return "Quantity Increase on order modification not allowed";
        case MessageCode::ONF:
            return "Order Not Found";
        }
    }

    void print_order(Order& o) {
        std::string order_side = (o.side == Side::BUY) ? "BUY" : "SELL";
        std::string order_type = (o.order_type == OrderType::LIMIT) ? "LIMIT" : (o.order_type == OrderType::MARKET) ? "MARKET" : "IOC";
        std::string order_status = (o.status == Status::Completed) ? "COMPLETED" : (o.status == Status::Rejected) ? "REJECTED" : "INPROGRESS";
        std::string order_message_code = get_message_from_code(o.msg_code);
        std::cout << "  ORDER: client_id=" << o.client_id
                << " order_id=" << o.order_id
                << " symbol=" << o.symbol
                << " side=" << order_side
                << " qty=" << o.quantity 
                << " rem_qty=" << o.remaining_quantity
                << " price=" << o.price
                << " order_type=" << order_type
                << " timestamp=" << o.arrival_time.time_since_epoch().count()
                << " status=" << order_status
                << " message_code=" << order_message_code
                << "\n";
    }
}