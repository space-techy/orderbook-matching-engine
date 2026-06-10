#include "matching_engine/helper.hpp"

namespace me::matching {

    std::string message_code_name(MessageCode m) {
        switch (m) {
            case MessageCode::OrderFilled:    return "OrderFilled";
            case MessageCode::OrderResting:   return "OrderResting";
            case MessageCode::OrderCancelled: return "OrderCancelled";
            case MessageCode::OrderModified:  return "OrderModified";
            case MessageCode::PartialFill:    return "PartialFill";
            case MessageCode::OrderRejected:  return "OrderRejected";
        }
        return "Unknown";
    }

    void print_order(const Order& o) { (void)o; }
    void print_trade(const Trade& t) { (void)t; }

}
