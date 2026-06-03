#pragma once

#include "matching_engine/matching_engine.hpp"
#include "matching_engine/glaze_meta.hpp"
#include <glaze/glaze.hpp>
#include <string>

namespace me::matching{
    inline std::optional<Message> parse_message(const std::string& raw){
        Message msg{};
        auto ec = glz::read_json(msg, raw);
        if(ec){
            return std::nullopt;
        }
        return msg;
    };

    inline std::string serialize_message(const Order& order){
        std::string out;
        auto ec = glz::write_json( order, out);
        if(ec){
            return R"({"error":"serialization_failed"})";
        }
        return out;
    };

    inline std::string serialize_message(const std::vector<Order>& orders){
        std::string out;
        auto ec = glz::write_json( orders, out);
        if(ec){
            return R"({"error":"serialization_failed"})";
        }
        return out;
    };


    inline std::string serialize_message(const Trade& trade){
        std::string out;
        auto ec = glz::write_json( trade, out);
        if(ec){
            return R"({"error":"serialization_failed"})";
        }
        return out;
    };

    inline std::string serialize_message(const std::vector<Trade>& trades){
        std::string out;
        auto ec = glz::write_json( trades, out);
        if(ec){
            return R"({"error":"serialization_failed"})";
        }
        return out;
    };
}