#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include "types.hpp"
#include "../utils/arena.hpp"

namespace lob {

struct OrderNode {
    OrderId id{};
    Side    side{};
    Price   price{};
    Qty     qty{};
    TsNs    ts{};
    OrderNode* prev{nullptr};
    OrderNode* next{nullptr};
};

struct PriceLevel {
    OrderNode* head{nullptr};
    OrderNode* tail{nullptr};

    bool empty() const { return head == nullptr; }

    void push_back(OrderNode* n) {
        n->prev = tail;
        n->next = nullptr;
        if (tail) tail->next = n;
        else head = n;
        tail = n;
    }

    void erase(OrderNode* n) {
        if (n->prev) n->prev->next = n->next;
        else head = n->next;
        if (n->next) n->next->prev = n->prev;
        else tail = n->prev;
        n->prev = n->next = nullptr;
    }

    OrderNode* front() const { return head; }
};

class OrderBook {
public:
    explicit OrderBook(Arena& arena, Instrument inst = {})
        : arena_(arena), inst_(std::move(inst)) {}

    // core APIs
    void on_new_order(const OrderId id, Side side, Price price, Qty qty,
                      TsNs ts, std::vector<Trade>& trades);

    void on_cancel_order(const OrderId id, TsNs ts);

    Price best_bid() const {
        return bids_.empty() ? Price{0} : bids_.begin()->first;
    }
    Price best_ask() const {
        return asks_.empty() ? Price{0} : asks_.begin()->first;
    }

    struct SnapshotLevel {
        Price price{};
        Qty   qty{};
    };

    struct Snapshot {
        std::vector<SnapshotLevel> bids;
        std::vector<SnapshotLevel> asks;
    };

    Snapshot snapshot(std::size_t depth = 5) const;

private:
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;

    Arena& arena_;
    Instrument inst_;
    BidMap bids_;
    AskMap asks_;

    struct IndexEntry {
        OrderNode* node{};
        PriceLevel* level{};
    };
    std::unordered_map<OrderId, IndexEntry> index_;

    void add_to_book(OrderNode* node, std::vector<Trade>& trades);
};

}
