#include "book.hpp"
#include <cmath>

namespace lob {

OrderBook::Snapshot OrderBook::snapshot(std::size_t depth) const {
    Snapshot snap;

    // bids
    for (auto it = bids_.begin(); it != bids_.end() && snap.bids.size() < depth; ++it) {
        Qty total = 0;
        for (auto n = it->second.head; n; n = n->next) total += n->qty;
        snap.bids.push_back({it->first, total});
    }

    // asks
    for (auto it = asks_.begin(); it != asks_.end() && snap.asks.size() < depth; ++it) {
        Qty total = 0;
        for (auto n = it->second.head; n; n = n->next) total += n->qty;
        snap.asks.push_back({it->first, total});
    }

    return snap;
}

void OrderBook::on_new_order(const OrderId id, Side side, Price price, Qty qty,
                             TsNs ts, std::vector<Trade>& trades) {
    if (qty <= 0) return;

    OrderNode* node = arena_.make<OrderNode>();
    node->id = id;
    node->side = side;
    node->price = price;
    node->qty = qty;
    node->ts = ts;

    add_to_book(node, trades);
}

void OrderBook::add_to_book(OrderNode* node, std::vector<Trade>& trades) {
    if (node->side == Side::Buy) {
        // match vs best asks
        while (node->qty > 0 && !asks_.empty() && node->price >= asks_.begin()->first) {
            auto it = asks_.begin();
            PriceLevel& level = it->second;
            OrderNode* maker = level.front();
            Qty traded = std::min(node->qty, maker->qty);

            Trade t;
            t.maker_id = maker->id;
            t.taker_id = node->id;
            t.taker_side = Side::Buy;
            t.price = it->first;
            t.qty = traded;
            t.ts = node->ts;
            trades.push_back(t);

            node->qty -= traded;
            maker->qty -= traded;

            if (maker->qty == 0) {
                level.erase(maker);
                index_.erase(maker->id);
                // no explicit delete: arena owns memory
            }
            if (level.empty()) {
                asks_.erase(it);
            }
        }

        if (node->qty > 0) {
            auto& level = bids_[node->price];
            level.push_back(node);
            index_[node->id] = {node, &level};
        }
    } else {
        // Sell side
        while (node->qty > 0 && !bids_.empty() && node->price <= bids_.begin()->first) {
            auto it = bids_.begin();
            PriceLevel& level = it->second;
            OrderNode* maker = level.front();
            Qty traded = std::min(node->qty, maker->qty);

            Trade t;
            t.maker_id = maker->id;
            t.taker_id = node->id;
            t.taker_side = Side::Sell;
            t.price = it->first;
            t.qty = traded;
            t.ts = node->ts;
            trades.push_back(t);

            node->qty -= traded;
            maker->qty -= traded;

            if (maker->qty == 0) {
                level.erase(maker);
                index_.erase(maker->id);
            }
            if (level.empty()) {
                bids_.erase(it);
            }
        }

        if (node->qty > 0) {
            auto& level = asks_[node->price];
            level.push_back(node);
            index_[node->id] = {node, &level};
        }
    }
}

void OrderBook::on_cancel_order(const OrderId id, TsNs /*ts*/) {
    auto it = index_.find(id);
    if (it == index_.end()) return;
    auto* node = it->second.node;
    auto* level = it->second.level;
    level->erase(node);
    if (level->empty()) {
        if (node->side == Side::Buy)
            bids_.erase(node->price);
        else
            asks_.erase(node->price);
    }
    index_.erase(it);
}

} 
