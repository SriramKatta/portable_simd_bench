#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

#include "bench/aligned_buffer.hpp"
#include "kernels/copy/copy.hpp"

namespace
{
    // Page alignment for both arrays: identical page offsets for src and dst
    // keep 4K-aliasing effects out of the comparison and make sizes comparable.
    constexpr std::size_t kAlignment = 4096;

    template <typename Backend>
    void bm_copy(benchmark::State &state)
    {
        const std::size_t n = static_cast<std::size_t>(state.range(0));
        aligned_buffer<double> src(n, kAlignment);
        aligned_buffer<double> dst(n, kAlignment);

        // First touch of every element, outside the timed loop.
        for (std::size_t i = 0; i < n; ++i)
        {
            src[i] = static_cast<double>(i);
            dst[i] = 0.0;
        }

        for (auto _ : state)
        {
            // Out-of-line call into the backend TU; nothing else is needed here.
            kernels::copy(Backend{}, dst.data(), src.data(), n);
        }

        const std::int64_t iterations = static_cast<std::int64_t>(state.iterations());
        state.SetItemsProcessed(iterations * static_cast<std::int64_t>(n));
        // Factor 2 follows the STREAM convention: one array read and one written;
        // the write-allocate traffic is not counted.
        state.SetBytesProcessed(iterations * static_cast<std::int64_t>(n) * 2 *
                                static_cast<std::int64_t>(sizeof(double)));
    }
}

// n from 2^8 to 2^24 in steps of x4; working set is 16*n bytes (L1 to well beyond L3).
#define PSB_REGISTER_COPY(name)                                              \
    [[maybe_unused]] static benchmark::Benchmark *const registered_##name =  \
        benchmark::RegisterBenchmark("copy/" #name, bm_copy<backends::name>) \
            ->RangeMultiplier(4)                                             \
            ->Range(1 << 8, 1 << 24);

PSB_FOR_EACH_BUILT_BACKEND(PSB_REGISTER_COPY)
