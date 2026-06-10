# matching-engine

A limit order-book matching engine in modern C++ with a WebSocket front-end.

Submission unit for a "Codeforces for quants" platform where competitors submit matching engines and quant strategies. This repository is *one such engine*.

---

## Status: in development

**This is a working version, not a production-grade engine.**

The current focus is *correctness and end-to-end flow* — orders in over WebSocket, matched in the core, results streamed back. Performance, protocol polish, and architectural cleanup are deliberately deferred. See [Needs Improvement](#needs-improvement) for the full list of known compromises.

The code is being written for learning; design choices favor clarity and "does it work" over "does it scale to a million orders/second." That trade-off is intentional at this stage.

---

## What it does

- Accepts JSON order messages over a WebSocket endpoint
- Maintains a per-symbol limit order book (bids on one side, asks on the other)
- Matches incoming orders against the opposite side using **price-time priority**
- Supports `new_order`, `cancel`, and `modify`
- Emits three kinds of output, all carrying a `type` discriminator:
  - **Order response** — sent to the submitting connection: the outcome of their request (filled / resting / cancelled / modified / partial / rejected)
  - **Resting-fill notice** — sent to a resting order's owner when someone else's aggressive order hits it (same `order_response` shape)
  - **Trade broadcast** — every fill, sent to all connections as a public market-data feed
- Single-threaded matching core fed by a thread-safe queue, with separate I/O threads on either side
- A monotonic `sequence_number` stamped on every order response (one per processed request; all consequences of request *N* share seq *N*)

---

## Architecture

```
                +------------------------+
                |  WebSocket (Crow)      |
                |  on_message handler    |
                +-----------+------------+
                            |
                            | parse JSON (Glaze)
                            v
                +------------------------+
                |  input_queue_          |   <-- ThreadSafeQueue<QueueItem>
                +-----------+------------+
                            |
                            | pop
                            v
                +------------------------+
                |  Matching worker       |   <-- single thread
                |  - SymbolTable lookup  |
                |  - OrderBook::match    |
                |  - OrderBook::add/     |
                |    cancel/modify       |
                +-----+--------+---------+
                      |        |
              push    |        |    push
                      v        v
        +-------------------+   +------------------------+
        | output_queue_     |   | broadcast_message_queue|
        +---------+---------+   +-----------+------------+
                  |                         |
                  | pop                     | pop
                  v                         v
        +-------------------+   +------------------------+
        | response_router   |   | broadcast_router       |
        | (per-conn send)   |   | (send to all conns)    |
        +-------------------+   +------------------------+
```

Each `OrderBook` is one symbol. The book holds:

- `buys_  : std::map<Ticks, std::list<Order>, std::greater<>>` — bids, highest first
- `sells_ : std::map<Ticks, std::list<Order>, std::less<>>`    — asks, lowest first
- `reference_order_ : std::unordered_map<OrderId, OrderLocation>` — O(1) lookup for cancel / modify, where `OrderLocation` holds a `std::list<Order>::iterator` directly into the price-level list

---

## Build

Requirements: CMake >= 3.20, a C++20 compiler (MSVC, GCC, or Clang).

Dependencies are fetched automatically via CMake `FetchContent`:

- [Crow](https://github.com/CrowCpp/Crow) — WebSocket + HTTP server
- [Glaze](https://github.com/stephenberry/glaze) — JSON serialization (compile-time reflection)
- [asio](https://github.com/chriskohlhoff/asio) — transitive dependency of Crow

```bash
cmake -S . -B build
cmake --build build
```

Output binary: `build/Debug/matching_server.exe` (Windows) or `build/matching_server` (Linux/Mac).

---

## Run

```bash
./build/Debug/matching_server.exe          # default port 3001
./build/Debug/matching_server.exe 4000     # custom port
```

The server listens for WebSocket connections on `/ws`.

---

## Protocol

JSON over WebSocket. Each client message is one of three operations.

## Client → Engine

### New order

```json
{
  "action": "new_order",
  "client_id": 1,
  "order_id": 1,
  "symbol": 1,
  "side": "buy",
  "order_type": "limit",
  "price": 100,
  "qty": 10
}
```

- `side`: `"buy"` or `"sell"`.
- `order_type`: `"limit"`, `"market"`, or `"ioc"`. Optional — defaults to `"limit"`. (Only limit semantics are currently implemented; see [Needs Improvement](#needs-improvement).)
- `price` must be `> 0`, `qty` must be `> 0`, else the order is rejected.

### Cancel

```json
{
  "action": "cancel",
  "client_id": 1,
  "order_id": 1,
  "symbol": 1
}
```

### Modify

```json
{
  "action": "modify",
  "client_id": 1,
  "order_id": 1,
  "symbol": 1,
  "qty": 5
}
```

Only quantity **decreases** are accepted. `qty` must be strictly less than the order's current `remaining_qty`. A modify that increases, leaves qty unchanged, or sets qty to 0 is rejected. The modify keeps the order's time priority (in-place qty reduction).

---

## Engine → Client

Every order-related frame uses **one envelope shape**, sent only to the relevant connection. A `trade_broadcast` (different shape) goes to all connections. The client distinguishes frames by the top-level `"type"` field.

### Order response envelope

```json
{
  "type": "order_filled",
  "sequence_number": 8848,
  "message_code": 0,
  "trades": [ /* Trade objects */ ],
  "orders": [ /* OrderStatus objects */ ],
  "error": ""
}
```

- `type` — human-readable discriminator (varies by outcome, see table below).
- `sequence_number` — monotonic counter, one per processed request. All frames produced by request *N* (the submitter's response **and** every resting owner's fill notice) carry the same number.
- `message_code` — the integer outcome code (0–5).
- `trades` — fills produced (empty if none).
- `orders` — affected order status rows. For the submitter this is their order's final state; for a resting owner it is their order's updated state.
- `error` — empty string unless `message_code == 5`, in which case it holds the reason.

**`OrderStatus` object:**
```json
{
  "order_id": 1,
  "client_id": 1,
  "symbol": 1,
  "side": "buy",
  "price": 100,
  "qty": 10,
  "remaining_qty": 0,
  "message_code": 0
}
```
`price` is always the **limit price as originally requested**. The execution price lives on the `Trade` (it may differ — you can fill at a better price than your limit).

**`Trade` object:**
```json
{
  "buyer_order_id": 1,
  "seller_order_id": 2,
  "buyer_client_id": 1,
  "seller_client_id": 2,
  "price": 100,
  "qty": 10,
  "symbol": 1
}
```

### Routing: who gets what

For one `new_order` that sweeps resting orders:

- **The submitter (aggressor)** gets **one** frame: `trades` holds every fill from the sweep; `orders` holds their own final status.
- **Each resting owner** whose order was hit gets **one frame per order of theirs filled**: `trades` holds the single fill that touched them; `orders` holds that order's new status. These frames arrive unprompted (the owner did not send a request this round). They share the aggressor's `sequence_number`.

A client tells "my request's response" apart from "my resting order got hit" by matching `orders[0].order_id` against its own outstanding-request vs resting-order bookkeeping.

### Trade broadcast (to all connections)

```json
{
  "type": "trade_broadcast",
  "symbol": 1,
  "price": 100,
  "qty": 10,
  "aggressor_side": "buy"
}
```

Public market data. One frame per fill. Carries no order IDs or client IDs — private information is never broadcast. No `sequence_number`.

### Parse failure

A frame that fails to parse gets a synchronous reply on the same connection and is **not** submitted to the engine:
```json
{ "type": "order_rejected", "error": "parse_failed" }
```

### Message codes

| Code | `type` string      | Meaning |
|------|--------------------|---------|
| 0    | `order_filled`     | Order fully filled, removed from book |
| 1    | `order_resting`    | New order accepted, resting in book (no fills) |
| 2    | `order_cancelled`  | Cancel acknowledged |
| 3    | `order_modified`   | Modify acknowledged (qty decreased in place) |
| 4    | `partial_fill`     | Some qty filled; remainder rests (or the resting order is partially consumed) |
| 5    | `order_rejected`   | Rejected — see `error` field |

**Rejection `error` strings:** `"qty must be > 0"`, `"price must be > 0"`, `"order not found"`, `"qty increase or no-op not allowed"`, `"invalid action"`, `"parse_failed"`.

---

## Design notes

A few choices that look unconventional are deliberate at this stage.

### `std::map<Price, std::list<Order>>` for price levels

Yes, a red-black tree of heap-allocated list nodes is *not* what production matching engines use. It is, however, the most direct way to get a working price-time priority book:

- The map's ordering gives "best bid" / "best ask" via `begin()` for free.
- The list-per-level preserves insertion order, which is exactly time priority.
- An `OrderId → list::iterator` lookup table makes cancel / modify O(log N) without scanning.

Performance is unmeasured. For the current milestone — *correct behavior end-to-end* — this is enough. The trade-off is acknowledged and tracked below.

### Single matching thread

All matching runs on one worker thread, fed by a mutex-and-condvar queue. This is the **right** choice — even in production, matching cores are single-threaded per symbol/shard to avoid lock contention on the hot path. The bottleneck is rarely the matching arithmetic; it's the queue plumbing and the data layout.

### Glaze for JSON

Compile-time reflection means no runtime hashing of field names and no `nlohmann::json` dynamic tree. It also means types must opt into `glz::meta` specializations, which is what `include/matching_engine/glaze_meta.hpp` does.

Because Glaze binds one `glz::meta` per C++ type, outbound JSON shapes that differ from the in-memory layout are expressed as small **envelope structs** (`OrderResponseEnvelope`, `TradeBroadcastEnvelope`). The serializer fills an envelope from the internal type, then writes that. This is how the `type` discriminator and the integer `message_code` get onto the wire without polluting the matching-core structs.

---

## Project layout

```
include/
  ThreadSafeQueue.hpp              # mutex + condvar queue
  matching_engine/
    type.hpp                       # ID aliases + Side / OrderType / MessageType / MessageCode enums
    order.hpp                      # Order, Message, Trade, OrderStatus, TradeBroadcast,
                                   #   OrderLocation, RestingFill, OrderResults, QueueItem, OutputMessage
    order_book.hpp                 # OrderBook class
    matching_engine.hpp            # MatchingEngine class + BroadcastBatch alias
    json_helper.hpp                # parse_message / serialize_order_response / serialize_trade_broadcast
    glaze_meta.hpp                 # wire envelopes + Glaze type metadata
  server/
    server.hpp                     # start_server entry point

src/
  main.cpp                         # binary entry point, signal handling
  server.cpp                       # Crow WS routes + router threads
  matching_engine.cpp              # worker loop, process_order, validation, message → order
  order_book.cpp                   # match_and_rest, cancel, modify

CMakeLists.txt
```

---

## Needs improvement

These are *known* gaps. They are not bugs in the working version — they are the deliberate next round of work.

### Data structures and performance

- **Price levels.** `std::map<Ticks, std::list<Order>>` works but is cache-hostile: every node is a separate heap allocation, scattered across memory. A production engine would use a **dense array of price levels** indexed by tick (since prices are bounded), with **intrusive linked lists** (no separate node allocation) and a **pre-allocated order pool**. Same algorithmic complexity, dramatically better cache behavior.
- **Symbol table.** `std::map<Symbol, OrderBook>` should at minimum be `std::unordered_map` (O(1) instead of O(log N)); ideally a flat array indexed by symbol ID resolved at session start.
- **Iterator vs index.** `OrderLocation` stores a `std::list<Order>::iterator` (a pointer under the hood). A pool-based design would store a 32-bit pool index instead — smaller, prefetch-friendly.

### Type system

- All IDs (`OrderId`, `ClientId`, `Symbol`, …) are aliases for `uint64_t`. The compiler cannot catch argument swaps. Should become **strong typedefs** — one-field structs distinct in the type system but zero-cost at runtime.

### Struct design

- `Order` and the other POD structs have no default member initializers. `Order o;` leaves primitive fields uninitialized. Every construction site currently sets all fields explicitly, but a default-initializer guard would be safer.
- `Message` is a tagged-union god struct — fields like `side` and `price` are meaningless on a `cancel`. Should become `std::variant<NewOrder, Cancel, Modify>`.

### Matching algorithm

- `match_and_rest()` in `order_book.cpp` duplicates the BUY and SELL paths nearly line-for-line. Should be deduplicated by templating on `Side`.
- **Self-trade prevention is not currently active.** It was prototyped (reject the incoming order if the best opposite level contains the same client) but is presently removed from the matching path. A correct version checks per-pair during the sweep, not just at the best level.
- `market` and `ioc` order types are defined in `OrderType` and accepted on the wire but not yet handled — the engine treats every order as `limit`.
- `modify` only supports in-place qty decrease (priority preserved). Price changes and qty increases — which on a real exchange reset time priority via cancel + re-insert — are rejected rather than handled.

### Transport / protocol

- **Sequence number, not correlation ID.** Every order response carries a monotonic `sequence_number`, which establishes a canonical processing order. It is *not* a per-request correlation ID echoed from the client — a bot still correlates responses to requests by `order_id`. A round-tripped `request_id` would be more direct.
- **Mixed channels.** Per-client order responses and public trade broadcasts both flow over the same WebSocket. Production exchanges separate these into private and public channels (different endpoints or topic subscriptions).
- **Ungraceful shutdown.** Frames queued for a connection that has closed are silently dropped. Should drain pending sends before completing the close.

### Code quality

- The struct field types and the `Trade.trade_id` counter exist but `trade_id` is not yet serialized to the wire — it is reserved for the validator. Drop it if the validator ends up not consuming it.

### Production benchmarks and patterns to study

The references that should guide the next phase:

- **LMAX Disruptor** — single-writer ring buffer, single-threaded core
- **NASDAQ ITCH/OUCH protocol specs** — public; the canonical exchange message taxonomy
- **CME iLink 3 / FIX** — self-trade prevention modes, advanced order types
- **Aeron / Chronicle Queue** — low-latency messaging libraries
- Ulrich Drepper, "What Every Programmer Should Know About Memory"

---