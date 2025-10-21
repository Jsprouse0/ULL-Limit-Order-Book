# ULL-Limit-Order-Book
A high-performance, deterministic in-memory **Limit Order Book (LOB)** simulator written in modern C++.  
The goal of this project is to build a low-latency matching engine and backtesting environment from scratch — no frameworks, no external dependencies — with full control over performance, determinism, and data structures.


## Current Goals

- Implement a **correct** price–time priority matching engine (single-threaded for now)
- Support **Add**, **Cancel**, and **Market** order types
- Maintain strict **price–time priority** using intrusive lists
- Replay event tapes from **CSV** files deterministically
- Write comprehensive **unit tests** for core book logic

---

