#include "matching_engine/matching_engine.hpp"

#include<iostream>
#include<sstream>
#include<string>
#include<chrono>
#include<cstdint>
#include<list>
#include<map>
#include<utility>
#include<vector>

namespace me::matching{
    void print_help() {
        std::cout << "\nCommands:\n"
                << "  new <client_id> <order_id> <symbol> <buy|sell> <price> <qty>\n"
                << "  cancel <client_id> <order_id> <symbol>\n"
                << "  modify <client_id> <order_id> <symbol> <new_qty>\n"
                << "  help\n"
                << "  quit\n\n";
    }

    void print_trade(const Trade& t) {
        std::cout << "  TRADE: buyer=" << t.buyer_order_id
                << " seller=" << t.seller_order_id
                << " price=" << t.price
                << " qty=" << t.quantity
                << " symbol=" << t.symbol << "\n";
    }

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
            return "Same quantity or Quantity Increase on order modification not allowed";
        case MessageCode::QZ:
            return "Quantity Zero Not allowed! You can cancel the order!";
        case MessageCode::ONF:
            return "Order Not Found";
        }
    }

    void print_order2(Order& o) {
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

    bool parse_line(const std::string& line, Message& msg) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        msg.arrival_time = std::chrono::system_clock::now();
        
        if (command == "new") {
            std::string side_str;
            if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol 
                    >> side_str >> msg.price >> msg.quantity)) {
                std::cout << "  parse error: new <client_id> <order_id> <symbol> <buy|sell> <price> <qty>\n";
                return false;
            }
            msg.type = MessageType::NEW_ORDER;
            msg.side = (side_str == "buy") ? Side::BUY : Side::SELL;
            msg.order_type = OrderType::LIMIT;
            return true;
        }
        else if (command == "cancel") {
            if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol)) {
                std::cout << "  parse error: cancel <client_id> <order_id> <symbol>\n";
                return false;
            }
            msg.type = MessageType::CANCEL;
            return true;
        }
        else if (command == "modify") {
            if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol >> msg.quantity)) {
                std::cout << "  parse error: modify <client_id> <order_id> <symbol> <new_price> <new_qty>\n";
                return false;
            }
            msg.type = MessageType::MODIFY;
            return true;
        }
        return false;
    }

}

int main(){
    using namespace me::matching;

    MatchingEngine engine;

    std::cout << "Matching Engine REPL — type 'help' for commands.\n";

    std::string line;
    while(true){
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        if (line == "quit" || line == "exit") break;
        if (line == "help") { print_help(); continue; }

        Message msg;
        if (!parse_line(line, msg)) continue;

        OrderResults result = engine.process_order(msg);
        std::cout << "  result message code: " << static_cast<int>(result.MessageCode) << "\n";
        for(auto &i: result.Trades){
            print_trade(i);
        }

        for(auto &i: result.Orders){
            print_order2(i);
        }
        std::cout << "\n";

    }

    std::cout << "Bye...\n";
    return 0;
}