#include "matching_engine/order_book.hpp"

#include<cstdint>
#include<chrono>
#include<list>
#include<map>
#include<utility>
#include<vector>
#include<iostream>
#include<sstream>

namespace me::matching{

    std::string OrderBook::get_message_from_code(MessageCode m){
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

    void OrderBook::print_order(Order& o) {
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

    std::pair<bool, MessageCode> OrderBook::add_order(Order& incoming_order){
        Side order_side = incoming_order.side;
        if(order_side == Side::BUY){
            buys_[incoming_order.price].push_back(incoming_order);
            std::list<Order>::iterator ref = std::prev(buys_[incoming_order.price].end());
            reference_order[incoming_order.order_id] = {
                incoming_order.side,
                incoming_order.price,
                ref
            };
            for(auto &price : buys_){
                std::cout << " price : " << price.first << " : orders : \n";
                for(auto &order: price.second){
                    print_order(order);
                }
            }
            return {1, MessageCode::OC};
        } else if(order_side == Side::SELL){
            sells_[incoming_order.price].push_back(incoming_order);
            std::list<Order>::iterator ref = std::prev(sells_[incoming_order.price].end());
            reference_order[incoming_order.order_id] = {
                incoming_order.side,
                incoming_order.price,
                ref
            };
            for(auto &price : sells_){
                std::cout << " price : " << price.first << " : orders : \n";
                for(auto &order: price.second){
                    print_order(order);
                }
            }
            return {1, MessageCode::OC};
        }
    }

    std::pair<bool, Order> OrderBook::cancel_order(OrderId order_id, ClientId client_id){
        auto cancel_order_id = reference_order.find(order_id);
        if(cancel_order_id != reference_order.end() && cancel_order_id->second.iter->client_id == client_id){
            auto order_info = cancel_order_id->second;
            Order order_cpy = *cancel_order_id->second.iter;
            if(order_info.side == Side::BUY){
                buys_[order_info.price].erase(order_info.iter);
            } else if(order_info.side == Side::SELL){
                sells_[order_info.price].erase(order_info.iter);
            }
            order_cpy.status = Status::Completed;
            order_cpy.msg_code = MessageCode::OC;
            reference_order.erase(cancel_order_id);
            return {1, order_cpy};
        }
        Order OrderNotFound;
        OrderNotFound.status = Status::Rejected;
        OrderNotFound.msg_code = MessageCode::ONF;
        return {0, OrderNotFound};
    }

    std::pair<bool, Order> OrderBook::modify_order(OrderId order_id, ClientId cliend_id, Order& order){
        Order OrderNotFound;
        auto modify_order_id = reference_order.find(order_id);
        if(modify_order_id != reference_order.end() && order.quantity > 0){
            if(modify_order_id->second.iter->remaining_quantity <= order.quantity){
                OrderNotFound.status = Status::Rejected;
                OrderNotFound.msg_code = MessageCode::QI;
                return {0, OrderNotFound};
            } else if(modify_order_id->second.iter->side != order.side){
                OrderNotFound.status = Status::Rejected;
                OrderNotFound.msg_code = MessageCode::SC;
                return {0, OrderNotFound};
            }
            modify_order_id->second.iter->quantity = order.quantity;
            modify_order_id->second.iter->remaining_quantity = order.quantity;
            return {0, *modify_order_id->second.iter};
        }
        OrderNotFound.status = Status::Rejected;
        OrderNotFound.msg_code = (order.quantity == 0) ? MessageCode::QZ : MessageCode::ONF;
        return {0, OrderNotFound};
    }

    std::map<Ticks, std::list<Order>, std::greater<>>::iterator OrderBook::best_bid(){
        auto best_bid_it = buys_.begin();
        return best_bid_it;
    }

    std::map<Ticks, std::list<Order>, std::less<>>::iterator OrderBook::best_ask(){
        auto best_ask_it = sells_.begin();
        return best_ask_it;
    }

    OrderResults OrderBook::match(Order& incoming_order){
        OrderResults order_results;
        Side order_side = incoming_order.side;
        Ticks order_price = incoming_order.price;
        std::vector<Trade> trades;
        std::vector<Order> orders;
        if(order_side == Side::BUY){
            auto best_ask_it = best_ask();
            if(best_ask_it != sells_.end() && best_ask_it->first <= order_price){
                for(auto &order_book_order: best_ask_it->second){
                    if(incoming_order.client_id == order_book_order.client_id){
                        incoming_order.status = Status::Rejected;
                        incoming_order.msg_code = MessageCode::SO;
                        order_results.Trades = trades;
                        order_results.Orders = {incoming_order};
                        order_results.MessageCode = MessageCode::SO;
                        return order_results;
                    }
                }
                while(sells_.size() > 0 && incoming_order.remaining_quantity > 0){
                    auto curr_best = best_ask();
                    if(curr_best->first > order_price)break;
                    // Iterating through all orders for a price
                    for(auto order_book_order = curr_best->second.begin(); order_book_order != curr_best->second.end() && incoming_order.remaining_quantity > 0;){
                        Trade trade;
                        order_book_order->status = Status::Completed;
                        incoming_order.status = Status::Completed;
                        if(incoming_order.remaining_quantity < order_book_order->remaining_quantity){
                            order_book_order->remaining_quantity -= incoming_order.remaining_quantity;
                            trade.quantity = incoming_order.remaining_quantity;
                            incoming_order.remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::PFO;
                            incoming_order.msg_code = MessageCode::FFO;
                            incoming_order.status = Status::Completed;
                            order_book_order->status = Status::INPROGRESS;
                        } else {
                            incoming_order.remaining_quantity -= order_book_order->remaining_quantity;
                            trade.quantity = order_book_order->remaining_quantity;
                            order_book_order->remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::FFO;
                            incoming_order.msg_code = MessageCode::PFO;
                            incoming_order.status = Status::INPROGRESS;
                            order_book_order->status = Status::Completed;
                        }
                        if(incoming_order.remaining_quantity == 0){
                            incoming_order.msg_code = MessageCode::FFO;
                            incoming_order.status = Status::Completed;
                        }
                        incoming_order.price = order_book_order->price;
                        orders.push_back(*order_book_order);
                        orders.push_back(incoming_order);
                        
                        trade.buyer_order_id = incoming_order.order_id;
                        trade.seller_order_id = order_book_order->order_id;
                        trade.trade_id = trade.buyer_order_id + trade.seller_order_id;
                        trade.price = order_book_order->price;
                        trade.symbol = order_book_order->symbol;
                        trade.timestamp = std::chrono::system_clock::now();
                        
                        trades.push_back(trade);
                        // auto cpy_order_book_order = order_book_order;
                        // order_book_order++;
                        if(order_book_order->remaining_quantity == 0){
                            reference_order.erase(order_book_order->order_id);
                            order_book_order = curr_best->second.erase(order_book_order);
                        } else {
                            order_book_order++;
                        }
                    }

                    // Removing the price
                    if(curr_best->second.size() == 0){
                        sells_.erase(curr_best);
                    }
                }
                incoming_order.price = order_price;
                orders.push_back(incoming_order);
                order_results.Trades = trades;
                order_results.Orders = orders;
                order_results.MessageCode = MessageCode::OC;
                return order_results;
            }
            incoming_order.status = Status::INPROGRESS;
            incoming_order.msg_code = MessageCode::REST;
            order_results.Trades = trades;
            order_results.Orders = {incoming_order};
            order_results.MessageCode = MessageCode::REST;
            return order_results;
        } else if(order_side == Side::SELL){
            auto best_bid_it = best_bid();
            if(best_bid_it != buys_.end() && best_bid_it->first >= order_price){
                for(auto &order_book_order: best_bid_it->second){
                    if(incoming_order.client_id == order_book_order.client_id){
                        incoming_order.status = Status::Rejected;
                        incoming_order.msg_code = MessageCode::SO;
                        order_results.Trades = trades;
                        order_results.Orders = {incoming_order};
                        order_results.MessageCode = MessageCode::SO;
                        return order_results;
                    }
                }
                while(buys_.size() > 0 && incoming_order.remaining_quantity > 0){
                    auto curr_best = best_bid();
                    if(curr_best->first < order_price) break;
                    // Iterating through all orders for a price
                    for(auto order_book_order = curr_best->second.begin(); order_book_order != curr_best->second.end() && incoming_order.remaining_quantity > 0;){
                        Trade trade;
                        order_book_order->status = Status::Completed;
                        incoming_order.status = Status::Completed;
                        if(incoming_order.remaining_quantity < order_book_order->remaining_quantity){
                            order_book_order->remaining_quantity -= incoming_order.remaining_quantity;
                            trade.quantity = incoming_order.remaining_quantity;
                            incoming_order.remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::PFO;
                            incoming_order.msg_code = MessageCode::FFO;
                            incoming_order.status = Status::Completed;
                            order_book_order->status = Status::INPROGRESS;
                        } else {
                            incoming_order.remaining_quantity -= order_book_order->remaining_quantity;
                            trade.quantity = order_book_order->remaining_quantity;
                            order_book_order->remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::FFO;
                            incoming_order.msg_code = MessageCode::PFO;
                            incoming_order.status = Status::INPROGRESS;
                            order_book_order->status = Status::Completed;
                        }
                        if(incoming_order.remaining_quantity == 0){
                            incoming_order.msg_code = MessageCode::FFO;
                            incoming_order.status = Status::Completed;
                        }
                        incoming_order.price = order_book_order->price;
                        orders.push_back(*order_book_order);
                        orders.push_back(incoming_order);
                        
                        trade.buyer_order_id = incoming_order.order_id;
                        trade.seller_order_id = order_book_order->order_id;
                        trade.trade_id = trade.buyer_order_id + trade.seller_order_id;
                        trade.price = order_book_order->price;
                        trade.symbol = order_book_order->symbol;
                        trade.timestamp = std::chrono::system_clock::now();
                        
                        trades.push_back(trade);
                        // auto cpy_order_book_order = order_book_order;
                        // order_book_order++;
                        if(order_book_order->remaining_quantity == 0){
                            reference_order.erase(order_book_order->order_id);
                            order_book_order = curr_best->second.erase(order_book_order);
                        } else {
                            order_book_order++;
                        }
                    }

                    // Removing the price
                    if(curr_best->second.size() == 0){
                        buys_.erase(curr_best);
                    }
                }
                incoming_order.price = order_price;
                orders.push_back(incoming_order);
                order_results.Trades = trades;
                order_results.Orders = orders;
                order_results.MessageCode = MessageCode::OC;
                return order_results;
            }
            incoming_order.status = Status::INPROGRESS;
            incoming_order.msg_code = MessageCode::REST;
            order_results.Trades = trades;
            order_results.Orders = {incoming_order};
            order_results.MessageCode = MessageCode::REST;
            return order_results;
        }

    }

}