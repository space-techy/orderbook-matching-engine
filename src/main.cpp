#include "matching_engine/matching_engine.hpp"
#include "matching_engine/helper.hpp"

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
            print_order(i);
        }
        std::cout << "\n";

    }

    std::cout << "Bye...\n";
    return 0;
}