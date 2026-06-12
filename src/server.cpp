#include "matching_engine/matching_engine.hpp"
#include "matching_engine/json_helper.hpp"
#include "server/server.hpp"
#include "crow.h"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace me::matching {

    void start_server(MatchingEngine& engine, int port) {
        ThreadSafeQueue<OutputMessage>  output_queue;
        ThreadSafeQueue<BroadcastBatch> broadcast_queue;
        engine.set_output_queue(&output_queue);
        engine.set_broadcast_queue(&broadcast_queue);

        std::map<ConnId, crow::websocket::connection*> connections;
        std::mutex                                     conn_mutex;
        std::atomic<ConnId>                            next_conn_id{1};

        crow::SimpleApp app;

        CROW_WEBSOCKET_ROUTE(app, "/ws")
            .onopen([&](crow::websocket::connection& conn) {
                const ConnId id = next_conn_id.fetch_add(1);
                conn.userdata(reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
                std::lock_guard lock(conn_mutex);
                connections[id] = &conn;
            })
            .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool) {
                const ConnId id = static_cast<ConnId>(reinterpret_cast<uintptr_t>(conn.userdata()));
                auto msg = parse_message(data);
                if (!msg.has_value()) {
                    conn.send_text(R"({"type":"order_rejected","error":"parse_failed"})");
                    return;
                }
                engine.submit(id, *msg);
            })
            .onclose([&](crow::websocket::connection& conn, const std::string&) {
                const ConnId id = static_cast<ConnId>(reinterpret_cast<uintptr_t>(conn.userdata()));
                std::lock_guard lock(conn_mutex);
                connections.erase(id);
            });

        std::thread response_router([&]() {
            while (true) {
                auto item = output_queue.pop_order();
                if (!item.has_value()) break;
                std::string out = serialize_order_response(*item);
                std::lock_guard lock(conn_mutex);
                auto it = connections.find(item->conn_id);
                if (it != connections.end()) {
                    it->second->send_text(out);
                }
            }
        });

        std::thread broadcast_router([&]() {
            while (true) {
                auto item = broadcast_queue.pop_order();
                if (!item.has_value()) break;
                if (item->empty()) continue;

                std::vector<std::string> payloads;
                payloads.reserve(item->size());
                for (const auto& tb : *item) {
                    payloads.push_back(serialize_trade_broadcast(tb));
                }

                std::lock_guard lock(conn_mutex);
                for (auto& [_, conn_ptr] : connections) {
                    for (const auto& p : payloads) conn_ptr->send_text(p);
                }
            }
        });

        app.port(port).multithreaded().run();

        output_queue.stop();
        broadcast_queue.stop();
        response_router.join();
        broadcast_router.join();
    }

}
