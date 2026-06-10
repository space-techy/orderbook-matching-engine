#pragma once

#include "matching_engine/matching_engine.hpp"
#include "matching_engine/glaze_meta.hpp"
#include <glaze/glaze.hpp>
#include <optional>
#include <string>

namespace me::matching {

    inline std::optional<Message> parse_message(const std::string& raw) {
        Message msg{};
        auto ec = glz::read_json(msg, raw);
        if (ec) return std::nullopt;
        return msg;
    }

    inline std::string side_to_string(Side s) {
        return s == Side::Buy ? "buy" : "sell";
    }

    inline std::string serialize_order_response(const OrderResults& r, SeqNum seq) {
        OrderResponseEnvelope env{
            type_for_code(r.message_code),
            seq,
            static_cast<int>(r.message_code),
            r.trades,
            r.orders,
            r.error
        };
        std::string out;
        if (glz::write_json(env, out)) {
            return R"({"type":"order_response","error":"serialization_failed"})";
        }
        return out;
    }

    inline std::string serialize_trade_broadcast(const TradeBroadcast& t) {
        TradeBroadcastEnvelope env{
            "trade_broadcast",
            t.symbol,
            t.price,
            t.qty,
            side_to_string(t.aggressor_side)
        };
        std::string out;
        if (glz::write_json(env, out)) {
            return R"({"type":"trade_broadcast","error":"serialization_failed"})";
        }
        return out;
    }

}
