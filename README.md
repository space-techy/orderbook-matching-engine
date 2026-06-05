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
- Supports `NEW_ORDER`, `CANCEL`, and `MODIFY`
- Self-trade prevention (an order from client X cannot match against client X's resting orders at the best level)
- Emits two kinds of output:
  - **Execution reports** — per-client result of their submitted order
  - **Trade broadcasts** — fills sent out as a public feed
- Single-threaded matching core fed by a thread-safe queue, with separate I/O threads on either side

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
- `reference_order : std::map<OrderId, OrderLocation>` — O(log N) lookup for cancel / modify, where `OrderLocation` holds a `std::list<Order>::iterator` directly into the price-level list

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

### New order

```json
{
  "action": "new_order",
  "client_id": 1,
  "order_id": 1,
  "symbol": 1,
  "side": "buy",
  "price": 100,
  "qty": 10
}
```

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

Only quantity decreases are accepted. A modify with a larger qty is rejected (`QI`). Side changes are rejected (`SC`).

### Server responses

The server emits two kinds of frames:

1. **Execution report** (to the submitting connection only) — array of `Order` objects reflecting state changes from this request.
2. **Trade broadcast** (to all connections) — array of `Trade` objects for any fills produced.

Both are JSON arrays. There is no top-level envelope or correlation ID in the current version — see [Needs Improvement](#needs-improvement).

### Message codes

| Code | Meaning |
|---|---|
| OC  | Order completed (matched / cancelled successfully) |
| REST | Order resting in book |
| FO  | First order in the book at this level |
| FFO | Fully filled order |
| PFO | Partially filled order |
| SO  | Self-order rejected |
| SC  | Modify rejected: side change |
| QI  | Modify rejected: quantity increase |
| ONF | Order not found (cancel / modify target missing) |

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

---

## Project layout

```
include/
  ThreadSafeQueue.hpp              # mutex + condvar queue
  matching_engine/
    type.hpp                       # ID / enum aliases
    order.hpp                      # Order, Trade, OrderResults, OrderLocation, Message
    order_book.hpp                 # OrderBook class
    matching_engine.hpp            # MatchingEngine class + queue item types
    helper.hpp                     # print helpers
    json_helper.hpp                # parse / serialize via Glaze
    glaze_meta.hpp                 # Glaze type metadata
  server/
    server.hpp                     # start_server entry point

src/
  main.cpp                         # binary entry point, signal handling
  server.cpp                       # Crow WS routes + router threads
  matching_engine.cpp              # worker loop, process_order, message → order
  order_book.cpp                   # match, add, cancel, modify
  helper.cpp                       # print helpers impl

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
- `MessageCode` mixes three orthogonal concepts (match outcomes, validation rejects, lookup failures). Should split into `MatchOutcome` and `RejectReason`, ideally combined as `std::variant`.
- Enum underlying types are `uint64_t` where `uint8_t` would do — wasted bytes in hot structs.

### Struct design

- `Order` has no default member initializers. `Order o;` leaves primitive fields uninitialized — already caused garbage values to leak into output. All POD fields should default-initialize.
- `Order` carries both the original request (immutable client contract) and live engine state (`remaining_quantity`, `status`, `msg_code`). Production engines split these into a request type and a book-entry type.
- `Message` is a tagged-union god struct — fields like `side` and `price` are meaningless on a `CANCEL`. Should become `std::variant<NewOrder, Cancel, Modify>`.

### Matching algorithm

- The `match()` function in `order_book.cpp` duplicates the BUY and SELL paths nearly line-for-line. Should be deduplicated by templating on `Side`.
- Self-trade prevention only checks the **best** price level. A client with orders at non-best levels can still self-trade once the match walks past the best. The check needs to be per-pair during matching, not pre-flight.
- `MARKET` and `IOC` order types are defined in `OrderType` but not yet handled — current implementation treats every order as `LIMIT`.
- `MODIFY` does not currently reset price-time priority when it should. Real exchanges reset priority on any price change or qty increase; only pure qty decrease preserves priority.

### Transport / protocol

- **No correlation IDs.** Responses arrive as a stream of frames with no link back to the request that triggered them. A bot doing naive request → recv → request cannot reliably correlate. Needs a `request_id` round-tripped from client to response.
- **Mixed channels.** Per-client execution reports and public trade broadcasts both flow over the same WebSocket. Production exchanges separate these into private and public channels (different endpoints or topic subscriptions).
- **Empty broadcasts.** The broadcast queue receives a `BroadcastMessage` even when `Trades` is empty (e.g., after a resting or cancel). These should be filtered out at the source.
- **No envelope.** Frames are raw `[orders]` or `[trades]` arrays with no `type` discriminator. Clients have to inspect structure to tell them apart.
- **Ungraceful shutdown.** Frames queued for a connection that has closed are silently dropped. Should drain pending sends before completing the close.

### Code quality

- Naming inconsistency across the codebase: `buys_`, `sells_`, `reference_order` (no trailing underscore), `SymbolTable` (PascalCase). Should pick one convention.
- `print_order` and `get_message_from_code` live in `helper.hpp` (improved from earlier placement on `OrderBook`), but they should ideally be `operator<<` overloads for proper stream integration.
- Several headers (`json_helper.hpp`, `glaze_meta.hpp`) have not been exercised by every translation unit they're meant to support; IDE-only errors have surfaced that the compiler hasn't yet seen.

### Production benchmarks and patterns to study

The references that should guide the next phase:

- **LMAX Disruptor** — single-writer ring buffer, single-threaded core
- **NASDAQ ITCH/OUCH protocol specs** — public; the canonical exchange message taxonomy
- **CME iLink 3 / FIX** — self-trade prevention modes, advanced order types
- **Aeron / Chronicle Queue** — low-latency messaging libraries
- Ulrich Drepper, "What Every Programmer Should Know About Memory"

---

## License

Not yet specified.
