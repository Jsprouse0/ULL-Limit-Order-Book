#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <string>
#include <vector>
#include <cmath>

#include "core/book.hpp"
#include "core/events.hpp"
#include "utils/ringbuffer.hpp"
#include "utils/time.hpp"

using namespace lob;

// Simple YAML-ish config parser
struct BacktestConfig {
    std::string symbol{"FOO"};
    std::size_t steps{10'000};
    double      mid_price{100.0};
    double      spread{0.01};
    double      sigma{0.1};
    double      mm_size{100.0};
    unsigned    seed{42};
};

BacktestConfig load_config(const std::string& path) {
    BacktestConfig cfg;
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open config, using defaults\n";
        return cfg;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        auto trim = [](std::string& s) {
            std::size_t b = s.find_first_not_of(" \t");
            std::size_t e = s.find_last_not_of(" \t");
            if (b == std::string::npos) { s.clear(); return; }
            s = s.substr(b, e - b + 1);
        };
        trim(key); trim(val);
        if (key == "symbol") cfg.symbol = val;
        else if (key == "steps") cfg.steps = static_cast<std::size_t>(std::stoull(val));
        else if (key == "mid_price") cfg.mid_price = std::stod(val);
        else if (key == "spread") cfg.spread = std::stod(val);
        else if (key == "sigma") cfg.sigma = std::stod(val);
        else if (key == "mm_size") cfg.mm_size = std::stod(val);
        else if (key == "seed") cfg.seed = static_cast<unsigned>(std::stoul(val));
    }
    return cfg;
}

// Strategy API
struct StrategyContext {
    OrderBook::Snapshot book;
    Price mid{};
    TsNs  ts{};
};

// Abstract plug-in
class Strategy {
public:
    virtual ~Strategy() = default;

    virtual void on_step(const StrategyContext& ctx,
                         std::vector<OrderEvent>& out_events) = 0;
};

// Very simple market-maker: keeps one bid/ask one tick away from mid
class MarketMakerStrategy : public Strategy {
public:
    explicit MarketMakerStrategy(const BacktestConfig& cfg)
        : cfg_(cfg) {}

    void on_step(const StrategyContext& ctx,
                 std::vector<OrderEvent>& out_events) override {
        // cancel any prior resting orders each step to keep it simple
        for (auto id : resting_ids_) {
            OrderEvent ev;
            ev.type = EventType::Cancel;
            ev.id = id;
            ev.ts = ctx.ts;
            out_events.push_back(ev);
        }
        resting_ids_.clear();

        Price tick = static_cast<Price>(std::round(cfg_.spread / 2.0 / cfg_.spread));
        Price mid_px = ctx.mid;

        Price bid_px = mid_px - static_cast<Price>(cfg_.spread * 0.5);
        Price ask_px = mid_px + static_cast<Price>(cfg_.spread * 0.5);

        // new bid
        OrderEvent b;
        b.type = EventType::New;
        b.id   = next_id_++;
        b.side = Side::Buy;
        b.price = bid_px;
        b.qty   = static_cast<Qty>(cfg_.mm_size);
        b.ts    = ctx.ts;
        out_events.push_back(b);
        resting_ids_.push_back(b.id);

        // new ask
        OrderEvent a = b;
        a.id    = next_id_++;
        a.side  = Side::Sell;
        a.price = ask_px;
        out_events.push_back(a);
        resting_ids_.push_back(a.id);
    }

private:
    BacktestConfig cfg_;
    OrderId next_id_{1};
    std::vector<OrderId> resting_ids_;
};

// Metrics
struct Metrics {
    double pnl{0.0};
    double max_drawdown{0.0};
    std::vector<double> equity_curve; // per step
    std::vector<double> returns;      // step-to-step
};

Metrics compute_metrics(const std::vector<double>& equity) {
    Metrics m;
    if (equity.empty()) return m;
    m.equity_curve = equity;

    double peak = equity.front();
    double prev = equity.front();

    for (std::size_t i = 0; i < equity.size(); ++i) {
        double v = equity[i];
        if (v > peak) peak = v;
        double dd = (peak - v);

        if (dd > m.max_drawdown) m.max_drawdown = dd;

        if (i > 0) {
            double r = (v - prev);
            m.returns.push_back(r);
        }
        prev = v;
    }

    m.pnl = equity.back() - equity.front();
    return m;
}

double sharpe_ratio(const Metrics& m) {
    if (m.returns.size() < 2) return 0.0;
    double mean = 0.0;

    for (double r : m.returns) mean += r;

    mean /= static_cast<double>(m.returns.size());
    double var = 0.0;

    for (double r : m.returns) {
        double d = r - mean;
        var += d * d;
    }

    var /= static_cast<double>(m.returns.size() - 1);
    double stddev = std::sqrt(var);

    if (stddev == 0.0) return 0.0;

    // treat each step as one "day" for simplicity
    return mean / stddev * std::sqrt(252.0);
}

// Backtester loop
int main(int argc, char** argv) {
    std::string config_path = argc > 1 ? argv[1] : "config.yaml";
    BacktestConfig cfg = load_config(config_path);

    Arena arena(8 << 20); // 8MB for orders
    Instrument inst{cfg.symbol, 1};
    OrderBook book(arena, inst);

    MarketMakerStrategy strat(cfg);

    std::mt19937_64 rng(cfg.seed);
    std::normal_distribution<double> noise(0.0, cfg.sigma);

    double mid = cfg.mid_price;
    double inventory = 0.0;
    double cash = 0.0;

    std::vector<double> equity;

    SpscRingBuffer<OrderEvent, 1 << 16> event_queue;
    std::vector<Trade> trades;

    for (std::size_t step = 0; step < cfg.steps; ++step) {
        TsNs ts = now_ns();
        // random mid-price dynamics
        mid += noise(rng);

        // snapshot
        auto snap = book.snapshot(5);
        StrategyContext ctx{snap, static_cast<Price>(std::round(mid)), ts};

        std::vector<OrderEvent> generated;
        strat.on_step(ctx, generated);

        // push into queue
        for (auto& ev : generated) {
            while (!event_queue.push(ev)) {
                // queue full; in real engine we'd handle back-pressure (come back to later?)
                OrderEvent tmp;
                event_queue.pop(tmp);
            }
        }

        // process events
        trades.clear();
        OrderEvent ev;
        while (event_queue.pop(ev)) {
            if (ev.type == EventType::New) {
                book.on_new_order(ev.id, ev.side, ev.price, ev.qty, ev.ts, trades);
            } else if (ev.type == EventType::Cancel) {
                book.on_cancel_order(ev.id, ev.ts);
            }
        }

        // PnL / inventory accounting
        for (const auto& t : trades) {
            double px = static_cast<double>(t.price);
            double q  = static_cast<double>(t.qty);

            if (t.taker_side == Side::Buy) {
                // strategy is maker on sell side if maker_id != 0, but
                // for simplicity I am assuming strategy is always maker
                cash += px * q;      // sold
                inventory -= q;
            } else {
                cash -= px * q;      // bought
                inventory += q;
            }
        }

        double mark_to_market = cash + inventory * mid;
        equity.push_back(mark_to_market);
    }

    Metrics metrics = compute_metrics(equity);
    double sharpe = sharpe_ratio(metrics);

    std::cout << "Backtest finished\n";
    std::cout << "  Steps:           " << cfg.steps << "\n";
    std::cout << "  Final PnL:       " << metrics.pnl << "\n";
    std::cout << "  Max drawdown:    " << metrics.max_drawdown << "\n";
    std::cout << "  Sharpe (simple): " << sharpe << "\n";

    return 0;
}
