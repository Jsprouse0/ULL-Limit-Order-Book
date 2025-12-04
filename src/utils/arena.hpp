#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <new>

namespace lob {

class Arena {
public:
    explicit Arena(std::size_t bytes = 1 << 20) { // default 1MB
        buffer_.resize(bytes);
        ptr_ = buffer_.data();
        end_ = buffer_.data() + buffer_.size();
    }

    void* allocate(std::size_t sz, std::size_t align = alignof(std::max_align_t)) {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(ptr_);
        std::uintptr_t aligned = (p + (align - 1)) & ~(align - 1);
        char* out = reinterpret_cast<char*>(aligned);
        if (out + sz > end_) {
            throw std::bad_alloc();
        }
        ptr_ = out + sz;
        return out;
    }

    template <typename T, typename... Args>
    T* make(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    void reset() {
        ptr_ = buffer_.data();
    }

private:
    std::vector<char> buffer_;
    char* ptr_{nullptr};
    char* end_{nullptr};
};

} // namespace lob
