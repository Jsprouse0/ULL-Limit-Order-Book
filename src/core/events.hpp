#pragma once
#include "types.hpp"

namespace lob {

enum class EventType : std::uint8_t {
    New,
    Cancel
};

struct OrderEvent {
    EventType type{};
    OrderId   id{};
    Side      side{};
    Price     price{};
    Qty       qty{};
    TsNs      ts{};
};

} 
