#include "matching_engine/matching_engine.hpp"
#include "matching_engine/helper.hpp"
#include "server/server.hpp"
#include "ThreadSafeQueue.hpp"

#include<iostream>
#include<sstream>
#include<string>
#include<chrono>
#include<cstdint>
#include<list>
#include<map>
#include<utility>
#include<vector>
#include <csignal>

// namespace me::matching{
//     void print_help() {
//         std::cout << "\nCommands:\n"
//                 << "  new <client_id> <order_id> <symbol> <buy|sell> <price> <qty>\n"
//                 << "  cancel <client_id> <order_id> <symbol>\n"
//                 << "  modify <client_id> <order_id> <symbol> <new_qty>\n"
//                 << "  help\n"
//                 << "  quit\n\n";
//     }

//     void print_trade(const Trade& t) {
//         std::cout << "  TRADE: buyer=" << t.buyer_order_id
//                 << " seller=" << t.seller_order_id
//                 << " price=" << t.price
//                 << " qty=" << t.quantity
//                 << " symbol=" << t.symbol << "\n";
//     }

//     bool parse_line(const std::string& line, Message& msg) {
//         std::istringstream iss(line);
//         std::string command;
//         iss >> command;
        
//         msg.arrival_time = std::chrono::system_clock::now();
        
//         if (command == "new") {
//             std::string side_str;
//             if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol 
//                     >> side_str >> msg.price >> msg.quantity)) {
//                 std::cout << "  parse error: new <client_id> <order_id> <symbol> <buy|sell> <price> <qty>\n";
//                 return false;
//             }
//             msg.type = MessageType::NEW_ORDER;
//             msg.side = (side_str == "buy") ? Side::BUY : Side::SELL;
//             msg.order_type = OrderType::LIMIT;
//             return true;
//         }
//         else if (command == "cancel") {
//             if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol)) {
//                 std::cout << "  parse error: cancel <client_id> <order_id> <symbol>\n";
//                 return false;
//             }
//             msg.type = MessageType::CANCEL;
//             return true;
//         }
//         else if (command == "modify") {
//             if (!(iss >> msg.client_id >> msg.order_id >> msg.symbol >> msg.quantity)) {
//                 std::cout << "  parse error: modify <client_id> <order_id> <symbol> <new_price> <new_qty>\n";
//                 return false;
//             }
//             msg.type = MessageType::MODIFY;
//             return true;
//         }
//         return false;
//     }

// }

namespace me::matching{
    MatchingEngine* g_engine = nullptr;
    void signal_handler(int) {
        if (g_engine) g_engine->stop();
    }
}

int main(int argc, char* argv[]){
    int port = 3001;
    if(argc > 1) port = std::stoi(argv[1]);

    using namespace me::matching;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    MatchingEngine engine;
    g_engine = &engine;
    engine.start();
    start_server(engine, 3001);
    engine.stop();
    std::cout << "Bye...\n";
    return 0;
}