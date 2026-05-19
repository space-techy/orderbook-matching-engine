#include "include/matching-engine/order_book.hpp"

namespace me::matching{

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
            return {1, MessageCode::OC};
        } else if(order_side == Side::SELL){
            sells_[incoming_order.price].push_back(incoming_order);
            std::list<Order>::iterator ref = std::prev(sells_[incoming_order.price].end());
            reference_order[incoming_order.order_id] = {
                incoming_order.side,
                incoming_order.price,
                ref
            };
            return {1, MessageCode::OC};
        }
    }

    std::pair<bool, MessageCode> OrderBook::cancel_order(OrderId order_id, ClientId client_id){
        auto cancel_order_id = reference_order.find(order_id);
        if(cancel_order_id != reference_order.end() && cancel_order_id->second.iter->client_id == client_id){
            auto order_info = cancel_order_id->second;
            if(order_info.side == Side::BUY){
                buys_[order_info.price].erase(order_info.iter);
            } else if(order_info.side == Side::SELL){
                sells_[order_info.price].erase(order_info.iter);
            }
            return {1, MessageCode::OC};
        }
        return {0, MessageCode::ONF};
    }

    std::pair<bool, MessageCode> OrderBook::modify_order(OrderId order_id, ClientId cliend_id, Order& order){
        auto modify_order_id = reference_order.find(order_id);
        if(modify_order_id != reference_order.end()){
            if(modify_order_id->second.iter->quantity > order.quantity){
                return {0, MessageCode::QI};
            } else if(modify_order_id->second.iter->side != order.side){
                return {0, MessageCode::SC};
            }
            modify_order_id->second.iter->quantity == order.quantity;
            return {0, MessageCode::OC};
        }
        return {0, MessageCode::ONF};
    }

    std::map<Ticks, std::list<Order>, std::greater<>>::iterator OrderBook::best_bid(){
        auto best_bid_it = buys_.begin();
        return best_bid_it;
    }

    std::map<Ticks, std::list<Order>, std::less<>>::iterator OrderBook::best_ask(){
        auto best_ask_it = sells_.begin();
        return best_ask_it;
    }

    std::pair<std::pair<std::vector<Trade>, std::vector<Order>>, MessageCode> OrderBook::match(Order& incoming_order){
        Side order_side = incoming_order.side;
        std::vector<Trade> trades;
        std::vector<Order> orders;
        if(order_side == Side::BUY){
            auto best_ask_it = best_ask();
            if(best_ask_it != sells_.end()){
                for(auto &order_book_order: best_ask_it->second){
                    if(incoming_order.client_id == order_book_order.client_id){
                        return {{trades, orders}, MessageCode::SO};
                    }
                }
                bool match_price = true;
                while(buys_.size() > 0 && incoming_order.remaining_quantity > 0 && match_price){
                    auto curr_best = best_ask();
                    // Iterating through all orders for a price
                    for(auto order_book_order = curr_best->second.begin(); order_book_order != curr_best->second.end();){
                        if(order_book_order->price > incoming_order.price){
                            match_price = false;
                            break;
                        }
                        Trade trade;
                        order_book_order->status = Status::Completed;
                        incoming_order.status = Status::Completed;
                        if(incoming_order.remaining_quantity < order_book_order->remaining_quantity){
                            order_book_order->remaining_quantity -= incoming_order.remaining_quantity;
                            trade.quantity = incoming_order.remaining_quantity;
                            incoming_order.remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::PFO;
                            incoming_order.msg_code = MessageCode::FFO;
                        } else {
                            incoming_order.remaining_quantity -= order_book_order->remaining_quantity;
                            trade.quantity = order_book_order->remaining_quantity;
                            order_book_order->remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::FFO;
                            incoming_order.msg_code = MessageCode::PFO;
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
                        auto cpy_order_book_order = order_book_order;
                        order_book_order++;
                        if(order_book_order->remaining_quantity == 0){
                            curr_best->second.erase(cpy_order_book_order);
                        }
                    }

                    // Removing the price
                    if(curr_best->second.size() == 0){
                        buys_.erase(curr_best);
                    }
                }
                return {{trades, orders}, MessageCode::OC};
            }
            return {{trades, orders}, MessageCode::FO};
        } else if(order_side == Side::SELL){
            auto best_bid_it = best_bid();
            if(best_bid_it != buys_.end()){
                for(auto &order_book_order: best_bid_it->second){
                    if(incoming_order.client_id == order_book_order.client_id){
                        return {{trades, orders}, MessageCode::SO};
                    }
                }
                bool match_price = true;
                while(buys_.size() > 0 && incoming_order.remaining_quantity > 0 && match_price){
                    auto curr_best = best_bid();
                    // Iterating through all orders for a price
                    for(auto order_book_order = curr_best->second.begin(); order_book_order != curr_best->second.end();){
                        if(order_book_order->price < incoming_order.price){
                            match_price = false;
                            break;
                        }
                        Trade trade;
                        order_book_order->status = Status::Completed;
                        incoming_order.status = Status::Completed;
                        if(incoming_order.remaining_quantity < order_book_order->remaining_quantity){
                            order_book_order->remaining_quantity -= incoming_order.remaining_quantity;
                            trade.quantity = incoming_order.remaining_quantity;
                            incoming_order.remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::PFO;
                            incoming_order.msg_code = MessageCode::FFO;
                        } else {
                            incoming_order.remaining_quantity -= order_book_order->remaining_quantity;
                            trade.quantity = order_book_order->remaining_quantity;
                            order_book_order->remaining_quantity = 0;
                            order_book_order->msg_code = MessageCode::FFO;
                            incoming_order.msg_code = MessageCode::PFO;
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
                        auto cpy_order_book_order = order_book_order;
                        order_book_order++;
                        if(order_book_order->remaining_quantity == 0){
                            curr_best->second.erase(cpy_order_book_order);
                        }
                    }

                    // Removing the price
                    if(curr_best->second.size() == 0){
                        sells_.erase(curr_best);
                    }
                }
                return {{trades, orders}, MessageCode::OC};
            }
            return {{trades, orders}, MessageCode::FO};
        }

    }

}