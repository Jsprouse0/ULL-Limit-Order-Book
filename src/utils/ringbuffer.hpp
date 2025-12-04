#pragma once
#include <atomic>
#include <cstddef>
#include <optional>

namespace lob {

// Single-producer / single-consumer ring buffer.
// Capacity must be power-of-two for cheaper masking.
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    SpscRingBuffer() : head_(0), tail_(0) {}

    bool push(const T& v) {
        auto head = head_.load(std::memory_order_relaxed);
        auto next = head + 1;
        if (next - tail_.load(std::memory_order_acquire) > Capacity) {
            return false; // full
        }
        buffer_[head & mask_] = v;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = buffer_[tail & mask_];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t mask_ = Capacity - 1;
    T buffer_[Capacity];
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
};

} 
