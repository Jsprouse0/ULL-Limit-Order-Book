#pragma once
#include <cstdint>
#include <string>

namespace lob {

using Price   = std::int64_t;
using Qty     = std::int64_t;
using OrderId = std::uint64_t;
using TsNs    = std::uint64_t; // nanoseconds since arbitrary epoch

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };

inline const char* side_to_str(Side s) {
    return s == Side::Buy ? "B" : "S";
}

struct Instrument {
    std::string symbol;
    Price tick_size{1};
};

struct Trade {
    OrderId maker_id{};
    OrderId taker_id{};
    Side    taker_side{};
    Price   price{};
    Qty     qty{};
    TsNs    ts{};
};

}
