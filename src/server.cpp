#include "matching_engine/matching_engine.hpp"
#include "matching_engine/json_helper.hpp"
#include "server/server.hpp"
#include "crow.h"
#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace me::matching{
    void start_server(MatchingEngine& engine, int port){
        ThreadSafeQueue<ProcessedResult> output_queue_;
        ThreadSafeQueue<BroadcastMessage> broadcast_message_queue_;
        engine.set_output_queue(&output_queue_);
        engine.set_broadcast_queue(&broadcast_message_queue_);


        // connection tracking
        std::map<ConnId, crow::websocket::connection*> connections;
        std::mutex conn_mutex;
        std::atomic<ConnId> next_conn_id{1};

        crow::SimpleApp app;

        CROW_WEBSOCKET_ROUTE(app, "/ws")
            .onopen([&](crow::websocket::connection& conn){
                ConnId id = next_conn_id++;
                conn.userdata(reinterpret_cast<void*>(id));
                std::lock_guard<std::mutex> lock(conn_mutex);
                connections[id] = &conn;
                std::cout << "[WS] Connection " << id << " opened\n";
            })
            .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool){
                ConnId id = reinterpret_cast<ConnId>(conn.userdata());
                try{
                    auto msg = parse_message(data);
                    if(!msg.has_value()){
                        std::string out_buffer;
                        Error error{"validation_failed", "The provided message structure or payload is invalid."};
                        if (glz::write_json(error, out_buffer)) {
                            conn.send_text(R"({"error":"internal_error","detail":"serialization_failed"})");
                        } else {
                            conn.send_text(out_buffer);
                        }
                    } else {
                        engine.submit(id, *msg);
                        // conn.send_text(R"("status" : "Order Recieved!")");
                    }
                } catch(const std::exception& e){
                    Error error{"parse_failed", e.what()};
                    std::string out = R"({"error" : "Internal Error"})";
                    auto ec = glz::write_json(error, out);
                    if(ec) {
                        std::cout <<  R"({"error":"serialization_failed_at_exception"})" << std::endl;
                    }
                    conn.send_text(out);
                }
            })
            .onclose([&](crow::websocket::connection& conn, const std::string&){
                ConnId id = reinterpret_cast<ConnId>(conn.userdata());
                std::lock_guard<std::mutex> lock(conn_mutex);
                connections.erase(id);
                std::cout << "[WS] Connection " << id << " closed\n";
            });
            
        
        std::thread response_router([&](){
            while(true){
                auto item = output_queue_.pop_order();
                if(!item.has_value()){
                    std::cout << "Output queue order stopped!" << std::endl;
                    break;
                }

                std::string output = serialize_message(item->order_results.Orders);
                std::lock_guard lock(conn_mutex);
                auto it = connections.find(item->conn_id);
                if(it != connections.end()){
                    it->second->send_text(output);
                }
            }
        });

        std::thread broadcast_router([&](){
            while(true){
                auto item = broadcast_message_queue_.pop_order();
                if(!item.has_value()){
                    std::cout << "broadcast_message_queue_ stopped!" << std::endl;
                    break;
                }
                if(item->trades.size() <= 0) continue;
                std::string output = serialize_message(item->trades);
                std::lock_guard lock(conn_mutex);
                for (auto& [id, conn_ptr] : connections) {
                    conn_ptr->send_text(output);
                }
            }
        });

            // ── Run server (blocks until shutdown) ──
        std::cout << "[Server] Starting on port " << port << "\n";
        app.port(port).multithreaded().run();

        output_queue_.stop();
        broadcast_message_queue_.stop();
        response_router.join();
        broadcast_router.join();
        std::cout << "[Server] Shutdown complete\n";
    };
}