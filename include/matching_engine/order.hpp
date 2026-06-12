#pragma once

#include "matching_engine/type.hpp"

#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>

namespace me::matching {

    // Resting order living in the book. order_id is the ENGINE's name for it
    // (dense internal counter); client_order_id is the CLIENT's name — the
    // ticket of the new_order request that created it. Stored here so every
    // fill can stamp outbound notices without a second lookup.
    struct Order {
        ClientId      client_id;
        OrderId       order_id;        // engine-internal book id
        ClientOrderId client_order_id; // ticket of the creating new_order request
        Symbol        symbol;
        Side          side;
        Qty           qty;             // original quantity at insertion
        Qty           remaining_qty;
        Ticks         price;
        OrderType     order_type;
        ConnId        conn_id;         // owner — for routing fill events back
    };

    // Incoming request from the wire. Every request carries its own fresh
    // client_order_id (the conversation ticket, echoed on the direct response).
    // cancel/modify additionally name WHICH order they target via the
    // client_order_id of the new_order that created it (FIX OrigClOrdID).
    struct Message {
        MessageType   type;
        ClientId      client_id;
        ClientOrderId client_order_id;        // this request's ticket — always fresh
        ClientOrderId target_client_order_id; // cancel/modify only; 0 when unused
        Symbol        symbol;
        Side          side;
        OrderType     order_type;
        Ticks         price;
        Qty           qty;
    };

    // Trade record — produced when an aggressor matches a resting order.
    // Carries BOTH namespaces: internal order ids (engine-private) and each
    // side's client_order_id (what bots and the validator speak natively).
    struct Trade {
        TradeId       trade_id;
        OrderId       buyer_order_id;
        OrderId       seller_order_id;
        ClientOrderId buyer_client_order_id;
        ClientOrderId seller_client_order_id;
        ClientId      buyer_client_id;
        ClientId      seller_client_id;
        Ticks         price;            // execution price (= resting order's price)
        Qty           qty;
        Symbol        symbol;
        Side          aggressor_side;   // who was the aggressor — needed for broadcast
    };

    // Per-order status row in an order_response.
    struct OrderStatus {
        OrderId       order_id;        // engine-internal book id (0 if never booked)
        ClientOrderId client_order_id; // ticket of the new_order that created it
        ClientId      client_id;
        Symbol        symbol;
        Side          side;
        Ticks         price;           // the LIMIT price as originally requested
        Qty           qty;
        Qty           remaining_qty;
        MessageCode   message_code;
    };

    // Public market data — no order_ids.
    struct TradeBroadcast {
        Symbol symbol;
        Ticks  price;
        Qty    qty;
        Side   aggressor_side;
    };

    // O(1) cancel support — direct iterator into the price level's list.
    struct OrderLocation {
        Side                       side;
        Ticks                      price;
        std::list<Order>::iterator iter;
    };

    // One resting order's fill event — produced inside match_and_rest, consumed
    // by the worker which turns each one into its own outbound OrderResults to
    // the resting owner. Same wire shape as the aggressor's response.
    struct RestingFill {
        ConnId      conn_id;       // routing key — resting owner's connection
        OrderStatus order;         // the resting owner's updated status row
        Trade       trade;         // the trade that affected them
    };

    // Aggregate outcome of processing one Message.
    // sequence_number is NOT here — stamped onto the envelope at serialize time.
    struct OrderResults {
        MessageCode               message_code;
        std::vector<Trade>        trades;
        std::vector<OrderStatus>  orders;
        std::vector<RestingFill>  resting_events;   // routed separately by worker
        std::string               error;            // empty unless rejected
    };

    // Queue payloads.
    struct QueueItem {
        ConnId  conn_id;
        Message message;
    };

    // Output queue payload — single shape now. The worker writes here for the
    // aggressor and for every resting order touched (with that owner's conn_id).
    //
    // The pairing contract lives in two optionals:
    //   client_order_id      set  → direct response to the request with that ticket
    //   client_order_id      null → unsolicited notice (your resting order was hit);
    //   orig_client_order_id then says WHICH of your orders it is about.
    // Null optionals are omitted from the wire — absence IS the signal.
    struct OutputMessage {
        ConnId                       conn_id;
        OrderResults                 payload;
        SeqNum                       sequence_number;
        std::optional<ClientOrderId> client_order_id;       // request echo; nullopt = unsolicited
        std::optional<ClientOrderId> orig_client_order_id;  // unsolicited notices only
    };

}
