#include "matching_engine/matching_engine.hpp"
#include "matching_engine/helper.hpp"
#include<cstdint>
#include<chrono>
#include<list>
#include<map>
#include<utility>
#include<vector>
#include<iostream>
#include<sstream>

namespace me::matching{

    OrderResults MatchingEngine::process_order(Message& order_message){
        OrderResults order_results{};
        MessageType message_type = order_message.type;
        OrderBook& order_book = SymbolTable[order_message.symbol];
        Order order_info = make_order_from_message(order_message);
        if(message_type == MessageType::CANCEL){
            std::pair<bool, Order> order_cancel_info = order_book.cancel_order( order_message.order_id, order_message.client_id);
            order_results.Trades = {};
            order_results.Orders = {order_cancel_info.second};
            order_results.MessageCode = order_cancel_info.second.msg_code;
            return order_results;
        } else if(message_type == MessageType::MODIFY){
            std::pair<bool, Order> order_modify_info = order_book.modify_order(order_message.order_id, order_message.client_id, order_info);
            order_results.Trades = {};
            order_results.Orders = {order_modify_info.second};
            order_results.MessageCode = order_modify_info.second.msg_code;
            return order_results;
        } else if(message_type == MessageType::NEW_ORDER){
            OrderResults order_match = order_book.match(order_info);
            if(order_match.MessageCode == MessageCode::REST || order_match.MessageCode == MessageCode::FO || (order_match.MessageCode == MessageCode::OC && order_match.Trades.size() == 0)){
                order_info.msg_code = MessageCode::REST;
                order_info.status = Status::INPROGRESS;
                order_book.add_order(order_info);
                std::cout << "In FO : ";
                print_order(order_info);
                std::cout << "Out\n";
            } else if(order_match.MessageCode == MessageCode::OC && order_match.Trades.size() > 0){
                if(order_info.remaining_quantity > 0){
                    order_info.status = Status::INPROGRESS;
                    order_info.msg_code = MessageCode::REST;
                    order_book.add_order(order_info);
                    std::cout << "In OC : ";
                    print_order(order_info);
                    std::cout << "Out\n";
                }
            }
            return order_match;
        }
    }

    Order MatchingEngine::make_order_from_message(Message& message){
        Order newOrder = {
            message.client_id,
            message.order_id,
            message.symbol,
            message.side,
            message.quantity,
            message.quantity,
            message.price,
            message.order_type,
            message.arrival_time,
            Status::INPROGRESS,
            MessageCode::REST,
        };
        return newOrder;
    }

}