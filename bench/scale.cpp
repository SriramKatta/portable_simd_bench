#include <benchmark/benchmark.h>

#include <cstddef>

#include "bench/stream.hpp"
#include "kernels/scale/scale.hpp"

namespace
{
    template <typename Backend>
    void bm_scale(benchmark::State &state)
    {
        const std::size_t n = static_cast<std::size_t>(state.range(0));
        stream_bench::stream_array src(n, stream_bench::source_value);
        stream_bench::stream_array dst(n, stream_bench::zero_value);

        for (auto _ : state)
        {
            // Out-of-line call into the backend TU; nothing else is needed here.
            kernels::scale(Backend{}, dst.data(), src.data(), stream_bench::kScalar, n);
        }

        // One array read and one written.
        stream_bench::set_counters(state, n, 2);
    }
}

#define PSB_REGISTER_SCALE(name)                                               \
    [[maybe_unused]] static benchmark::Benchmark *const registered_##name =    \
        benchmark::RegisterBenchmark("scale/" #name, bm_scale<backends::name>) \
            ->Apply(stream_bench::sizes);

PSB_FOR_EACH_BUILT_BACKEND(PSB_REGISTER_SCALE)
