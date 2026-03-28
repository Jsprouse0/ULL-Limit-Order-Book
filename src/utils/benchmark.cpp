#include "benchmark.hpp"

namespace lob {
    Benchmark::Benchmark(const std::string& name) : name_(name) {
        start();
    }

    Benchmark::~Benchmark() {
        if(is_running_) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count();
            std::cout << "Benchmark [" << name_ << "] took " << duration << "ns\n";
        }
    }

    void Benchmark::start() {
        start_time_ = std::chrono::steady_clock::now();
        is_running_ = true;
    }

    void Benchmark::reset() {
        start_time_ = std::chrono::steady_clock::now();
    }

    inline std::uint64_t Benchmark::run_timer_ns() {
        if(!is_running_) {
            std::cerr << "Benchmark [" << name_ << "] is not running. Returning 0.\n";
            return 0;
        }

        auto end_time{std::chrono::steady_clock::now()};
        auto duration{std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count()};
        
        start_time_ = end_time; // reset start time for next run

        return duration;
    }

    bool Benchmark::running() const {
        return is_running_;
    }
}
