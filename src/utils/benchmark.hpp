#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <iostream>
#include <utility>

namespace lob {
    class Benchmark {
        public:
            explicit Benchmark(const std::string& name);
            ~Benchmark();
            void start();
            void reset();
            bool running() const;
            inline std::uint64_t run_timer_ns();

        private:
            std::string name_;
            std::chrono::steady_clock::time_point start_time_{};
            bool is_running_{false};
    };
}

#endif // BENCHMARK_HPP