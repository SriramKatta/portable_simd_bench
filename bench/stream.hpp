#pragma once

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

#include "bench/aligned_buffer.hpp"

// What the STREAM kernel benchmarks (bench/<kernel>.cpp) have in common.
namespace stream_bench
{
    // Page alignment for every array: identical page offsets keep 4K-aliasing effects
    // out of the comparison and make sizes comparable.
    constexpr std::size_t kAlignment = 4096;

    // An array of n doubles with every element touched once (first touch), so that page
    // faults happen here and not in the timed loop. The element at index i is init(i).
    class stream_array : public aligned_buffer<double>
    {
    public:
        template <typename F>
        stream_array(std::size_t n, F init) : aligned_buffer<double>(n, kAlignment)
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                (*this)[i] = init(i);
            }
        }
    };

    // Values for the source arrays: finite, never zero or denormal, small enough that no
    // kernel overflows.
    inline double source_value(std::size_t i)
    {
        return 1.0 + static_cast<double>(i % 1024);
    }

    // Value for the destination arrays, which only need their first touch.
    inline double zero_value(std::size_t)
    {
        return 0.0;
    }

    // The scalar of scale and triad.
    constexpr double kScalar = 3.0;

    // Counters. `arrays` is the number of arrays read or written per element (2 for copy
    // and scale, 3 for add and triad), which is the STREAM convention: every array counts
    // once, the write-allocate traffic is not counted.
    inline void set_counters(benchmark::State &state, std::size_t n, std::int64_t arrays)
    {
        const std::int64_t iterations = static_cast<std::int64_t>(state.iterations());
        state.SetItemsProcessed(iterations * static_cast<std::int64_t>(n));
        state.SetBytesProcessed(iterations * static_cast<std::int64_t>(n) * arrays *
                                static_cast<std::int64_t>(sizeof(double)));
    }

    // n from 2^8 to 2^24 in steps of x4; the working set is 8 * n bytes per array
    // (L1 to well beyond L3).
    inline void sizes(benchmark::Benchmark *b)
    {
        b->RangeMultiplier(4)->Range(1 << 8, 1 << 24);
    }
}
