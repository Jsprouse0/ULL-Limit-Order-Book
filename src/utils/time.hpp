#pragma once
#include <chrono>
#include <cstdint>

namespace lob {

inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

} 
