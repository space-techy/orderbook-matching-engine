# Matching Engine — Detailed Technical Description

## Purpose

This is a **reference matching engine** that serves two roles:
1. **Contestant's target:** Contestants implement their own matching engine. Ours is the reference implementation that defines correct behavior.
2. **Validator engine:** After a test, we replay the same orders through this engine to verify the contestant's outputs were correct.

It also serves as the development/testing target for the bot fleet during local development.

## Architecture Overview

```
                    ┌──────────────────────────────────────────┐
                    │              Crow WebSocket Server        │
                    │              (server.cpp)                 │
                    │                                          │
   Bot connects →   │  onopen: register conn_id                │
   via WebSocket    │  onmessage: parse JSON → engine.submit() │
                    │  onclose: unregister conn_id             │
                    └───────────────┬───────────────────────────┘
                                    │ pushes to
                                    ▼
                    ┌──────────────────────────────────────────┐
                    │         ThreadSafeQueue<InputItem>        │
                    │         (input queue)                     │
                    └───────────────┬───────────────────────────┘
                                    │ single worker pops from
                                    ▼
                    ┌──────────────────────────────────────────┐
                    │           MatchingEngine                  │
                    │           (matching_engine.cpp)           │
                    │                                          │
                    │  process_order(msg) →                    │
                    │    looks up symbol → OrderBook            │
                    │    delegates to OrderBook methods         │
                    │    returns OrderResults                   │
                    └───────────────┬───────────────────────────┘
                                    │ pushes results to
                              ┌─────┴─────┐
                              ▼           ▼
                    ┌──────────────┐  ┌──────────────────┐
                    │ output_queue  │  │ broadcast_queue   │
                    │ (per-conn)   │  │ (all connections) │
                    └──────┬───────┘  └──────┬────────────┘
                           ▼                 ▼
                    Response Router      Broadcast Router
                    (sends to the        (sends to ALL
                     specific conn       connected clients)
                     that sent the       — market data feed)
                     order)
```

### Threading Model (4 threads)

1. **Crow event loop thread:** Handles all WebSocket connections. Parses incoming JSON, pushes to input queue, returns immediately. NEVER blocks on matching.
2. **Worker thread:** Single thread that pops from input queue, runs matching logic, pushes results to output and broadcast queues. This is the ONLY thread that touches OrderBook state.
3. **Response router thread:** Pops from output queue, sends the result back to the specific connection that sent the order.
4. **Broadcast router thread:** Pops from broadcast queue, sends market data updates to ALL connected clients.

Single-writer principle: only the worker thread reads/writes OrderBook. No locks on the book itself.

## Data Types and Structs

### Core Types (type.hpp)

```cpp
using Ticks = int32_t;       // Price in integer ticks (no floats!)
using Qty = uint32_t;        // Order quantity
using OrderId = uint64_t;    // Unique order identifier (assigned by the CLIENT/bot, not the engine)
using ClientId = uint64_t;   // Identifies which client/bot sent the order
using Symbol = uint32_t;     // Identifies the instrument (e.g., symbol 1, symbol 2)
using ConnId = uint64_t;     // Internal: which WebSocket connection

enum class Side : uint8_t { BUY, SELL };
enum class MessageType : uint8_t { NEW_ORDER, CANCEL, MODIFY };
```

### Order Struct (order.hpp)

```cpp
// Order = a resting order sitting IN the order book
struct Order {
    ClientId client_id;
    OrderId order_id;
    Symbol symbol;
    Side side;
    Qty qty;           // original quantity
    Qty remaining_qty; // how much is left unfilled
    Ticks price;
    // TODO: add ConnId so we can send fill notifications to the resting order's owner
    // TODO: add timestamp for proper time priority
};
```

### Message Struct (order.hpp)

```cpp
// Message = an incoming REQUEST to the engine (not the same as Order!)
// A message becomes an Order if it rests in the book
struct Message {
    MessageType type;    // NEW_ORDER, CANCEL, or MODIFY
    ClientId client_id;
    OrderId order_id;
    Symbol symbol;
    Side side;
    Ticks price;
    Qty qty;
};
```

### Trade Struct (order.hpp)

```cpp
struct Trade {
    OrderId buyer_order_id;
    OrderId seller_order_id;
    ClientId buyer_client_id;
    ClientId seller_client_id;
    Ticks price;      // trade executes at the RESTING order's price
    Qty qty;           // quantity traded
    Symbol symbol;
};
```

### OrderResults Struct (order.hpp)

```cpp
// The result of processing one Message
struct OrderResults {
    int message_code;           // 0=success, 1=resting, 5=partial?, etc.
    std::vector<Trade> trades;  // trades generated (empty if order rested)
    std::vector<Order> orders;  // order status updates
};
```

### OrderLocation (order.hpp) — O(1) Cancel Support

```cpp
struct OrderLocation {
    Side side;
    Ticks price;
    std::list<Order>::iterator it;  // direct pointer into the price level's order list
};
```

The `reference_order` map (`unordered_map<OrderId, OrderLocation>`) stores one entry per resting order. When a cancel comes in, we look up the OrderId → get the iterator → erase from the list in O(1). No linear scan.

## OrderBook (order_book.hpp / order_book.cpp)

```cpp
class OrderBook {
    // Bids sorted descending (highest price first = best bid)
    std::map<Ticks, std::list<Order>, std::greater<>> buys_;
    
    // Asks sorted ascending (lowest price first = best ask)  
    std::map<Ticks, std::list<Order>, std::less<>> sells_;
    
    // O(1) lookup: order_id → {side, price, iterator}
    std::unordered_map<OrderId, OrderLocation> reference_order;

public:
    OrderResults process_new_order(const Message& msg);
    OrderResults cancel_order(const Message& msg);
    OrderResults modify_order(const Message& msg);
};
```

### Matching Logic (process_new_order)

1. Incoming buy: check if buy.price >= best_ask.price (prices cross)
2. If yes: match against asks, starting at best ask, walking up price levels
3. At each level: fill orders front-to-back (FIFO = time priority)
4. Partial fills: reduce remaining_qty, order stays in book
5. Full fills: remove order from book and reference_order map
6. If incoming order still has remaining_qty after matching: rest in book
7. Generate Trade structs for each fill

### Self-Trade Prevention (STP-CN)

Before matching, check if buyer_client_id == seller_client_id. If same client on both sides AND prices cross: cancel the NEWER order (the incoming one). The resting order stays.

### Cancel Logic

1. Look up order_id in reference_order → get OrderLocation
2. If not found: return error "order not found"
3. Use the stored iterator to erase from the list in O(1)
4. If the list at that price level is now empty, erase the price level from the map
5. Remove from reference_order

### Modify Logic

- Decrease qty only: update in-place, KEEP time priority
- Increase qty or change price: cancel + re-insert (LOSES time priority)
- Side change: reject (forbidden)

## MatchingEngine (matching_engine.hpp / matching_engine.cpp)

```cpp
class MatchingEngine {
    std::map<Symbol, OrderBook> symbol_table_;  // one OrderBook per symbol
    ThreadSafeQueue<InputItem> input_queue_;
    ThreadSafeQueue<ProcessedResult>* output_queue_;     // owned by server
    ThreadSafeQueue<ProcessedResult>* broadcast_queue_;  // owned by server
    std::thread worker_;
    std::atomic<bool> running_;

public:
    void start();                              // launches worker thread
    void stop();                               // signals stop, joins worker
    void submit(ConnId conn_id, Message msg);  // pushes to input queue (non-blocking)
    
private:
    void worker_loop();                        // pops from input, processes, pushes to output
    OrderResults process_order(const Message& msg);  // delegates to appropriate OrderBook
};
```

## Server (server.cpp)

Uses Crow library for HTTP + WebSocket in one server.

```cpp
void start_server(MatchingEngine& engine, int port) {
    crow::SimpleApp app;
    
    // Connection registry: conn_id → connection pointer
    std::unordered_map<ConnId, crow::websocket::connection*> connections;
    std::mutex conn_mutex;
    
    // Output + broadcast queues (owned by server, engine points to them)
    ThreadSafeQueue<ProcessedResult> output_queue;
    ThreadSafeQueue<ProcessedResult> broadcast_queue;
    
    // WebSocket route
    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
            // Register connection
        })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool) {
            // Parse JSON → Message, call engine.submit(conn_id, msg)
            // Returns immediately — does NOT wait for matching result
        })
        .onclose([&](crow::websocket::connection& conn, const std::string&) {
            // Unregister connection
        });
    
    // Response router thread: sends results back to the specific connection
    std::thread response_router([&]() {
        while (true) {
            auto result = output_queue.pop();
            if (!result) break;
            // Find connection by conn_id, serialize result to JSON, send
        }
    });
    
    // Broadcast router thread: sends market data to ALL connections
    std::thread broadcast_router([&]() {
        while (true) {
            auto result = broadcast_queue.pop();
            if (!result) break;
            // Serialize, send to ALL registered connections
        }
    });
    
    app.port(port).multithreaded().run();
}
```

## JSON Serialization (Glaze)

Uses the Glaze library for compile-time JSON serialization. Meta specializations are in `glaze_meta.hpp` and must be in the GLOBAL namespace:

```cpp
template<> struct glz::meta<me::matching::Message> {
    using T = me::matching::Message;
    static constexpr auto value = object(
        "action", &T::type,
        "client_id", &T::client_id,
        "order_id", &T::order_id,
        // ...
    );
};
```

`json_helpers.hpp` contains `parse_message()` and `serialize_results()` helper functions.

## WebSocket Protocol (Bot ↔ Engine)

### Bot sends to Engine:

**New Order:**
```json
{
    "action": "new_order",
    "client_id": 1,
    "order_id": 1000042,
    "symbol": 1,
    "side": "buy",
    "price": 995,
    "qty": 15
}
```

**Cancel:**
```json
{
    "action": "cancel",
    "client_id": 1,
    "order_id": 1000042,
    "symbol": 1
}
```

**Modify:**
```json
{
    "action": "modify",
    "client_id": 1,
    "order_id": 1000042,
    "symbol": 1,
    "qty": 5
}
```

### Engine responds to Bot:

**Order rested (no match):**
```json
{
    "message_code": 1,
    "sequence_number": 8847,
    "trades": [],
    "orders": [
        {
            "order_id": 1000042,
            "client_id": 1,
            "symbol": 1,
            "side": "buy",
            "price": 995,
            "qty": 15,
            "remaining_qty": 15,
            "status": "resting",
            "message_code": 1
        }
    ]
}
```

**Order matched (trade happened):**
```json
{
    "message_code": 0,
    "sequence_number": 8848,
    "trades": [
        {
            "buyer_order_id": 1000042,
            "seller_order_id": 2000015,
            "buyer_client_id": 1,
            "seller_client_id": 2,
            "price": 995,
            "qty": 15,
            "symbol": 1
        }
    ],
    "orders": [
        {
            "order_id": 1000042,
            "client_id": 1,
            "status": "filled",
            "remaining_qty": 0,
            "message_code": 0
        }
    ]
}
```

**Cancel acknowledged:**
```json
{
    "message_code": 2,
    "sequence_number": 8849,
    "trades": [],
    "orders": [
        {
            "order_id": 1000042,
            "client_id": 1,
            "status": "cancelled",
            "message_code": 2
        }
    ]
}
```

### Engine broadcasts to ALL connections (market data):

**Trade broadcast:**
```json
{
    "type": "trade",
    "symbol": 1,
    "price": 995,
    "qty": 15,
    "buyer_order_id": 1000042,
    "seller_order_id": 2000015
}
```

### Fill notification to resting order's owner:

When an aggressive order matches a resting order, the resting order's owner must be notified. This is a server-initiated push to the connection that placed the resting order.

```json
{
    "type": "fill_notification",
    "order_id": 2000015,
    "client_id": 2,
    "fill_price": 995,
    "fill_qty": 15,
    "remaining_qty": 0,
    "status": "filled"
}
```

**IMPORTANT:** This requires storing the `ConnId` in the `Order` struct when it rests in the book, so we know where to send the fill notification later. This is currently NOT implemented and needs to be added.

## Known Issues and TODOs

### Critical for Correctness
- [ ] **Store ConnId in Order:** Needed for fill notifications to resting order owners
- [ ] **Add timestamp to Order:** Needed for proper time priority verification
- [ ] **Add sequence_number to responses:** Monotonically increasing counter per engine. Validator uses this to establish canonical processing order.
- [ ] **Structured response format:** Currently responses are a flat array. Should be `{"message_code": N, "sequence_number": N, "trades": [...], "orders": [...]}` as documented above.

### For Better Debugging
- [ ] **Debug logging per order:** Log what was received, what the book state was, what the result was. Should be toggleable (not always on — affects performance).
- [ ] **Book state dump:** Endpoint or function to dump current book state (all price levels, all orders) for debugging.
- [ ] **Order count tracking:** Track total orders processed, total trades, total cancels, current book depth per side.

### For Production
- [ ] **Error responses:** What happens when cancel targets non-existent order? When modify has invalid qty? Currently unclear — should return structured error with error_code.
- [ ] **Connection cleanup:** When a connection closes, what happens to its resting orders? Options: leave them (realistic), cancel all (safer for testing).
- [ ] **Graceful shutdown:** Stop accepting new connections, drain queues, then exit.

## Build Instructions

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./matching_server  # starts on port 3001
```

Dependencies (fetched via CMake FetchContent):
- Crow (WebSocket/HTTP server)
- Glaze (JSON serialization)

## Message Code Reference

```
0  = OC  (Order Completed / Fully Filled)
1  = OR  (Order Resting / Accepted)
2  = OX  (Order Cancelled)
3  = OM  (Order Modified)
4  = PF  (Partial Fill)
5  = ??  (Currently used for some matching responses — needs cleanup)
-1 = ERR (Error — order not found, invalid parameters, etc.)
```

These codes need to be standardized. The current implementation is inconsistent.
