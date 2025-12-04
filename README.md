# Ultra-Low-Latency Limit Order Book / Exchange Simulator

This project is a small in-memory exchange simulator intended as a
portfolio piece and a base for experimentation.

## High-level design

- **Core matching engine**
  - `OrderBook` keeps separate bid/ask maps.
  - Each price level uses **intrusive doubly-linked lists** of `OrderNode`s to
    maintain strict price–time priority.
  - Orders are stored in an `Arena` allocator to avoid per-order heap traffic.
  - An `unordered_map<OrderId, OrderNode*>` index gives O(1) cancel by id.

- **Lock-free queues**
  - `SpscRingBuffer<T, N>` in `utils/ringbuffer.hpp` is a single-producer /
    single-consumer lock-free ring buffer.
  - The backtest uses it to enqueue `OrderEvent`s into the engine.

- **Timestamps**
  - All events and trades carry nanosecond-resolution timestamps from
    `utils/time.hpp` (`now_ns()`).

- **Strategy plug-in API**
  - `Strategy` abstract base class with `on_step(...)`.
  - `MarketMakerStrategy` implements a simple symmetric market-making
    strategy adding bid/ask around a stochastic mid-price.
  - The API is intended to be extended for VWAP, TWAP, stat-arb, etc.

- **Deterministic backtesting**
  - `main.cpp` loads a small YAML-style config (`config.yaml`) with fields like
    `steps`, `seed`, `mid_price`, etc.
  - Randomness comes from `std::mt19937_64` seeded from config for
    reproducibility.
  - The loop records an equity curve and computes PnL, max drawdown,
    and a simple Sharpe ratio.

## Building

You can build this with any C++17 compiler. Example with `g++`:

```bash
g++ -std=c++17 -O3 -I./src \
  src/utils/arena.cpp \
  src/utils/ringbuffer.cpp \
  src/utils/time.cpp \
  src/core/types.cpp \
  src/core/events.cpp \
  src/core/book.cpp \
  src/main.cpp \
  -o lob_sim
