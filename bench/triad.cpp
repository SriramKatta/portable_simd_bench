#include <benchmark/benchmark.h>

#include <cstddef>

#include "bench/stream.hpp"
#include "kernels/triad/triad.hpp"

namespace
{
    template <typename Backend>
    void bm_triad(benchmark::State &state)
    {
        const std::size_t n = static_cast<std::size_t>(state.range(0));
        stream_bench::stream_array a(n, stream_bench::source_value);
        stream_bench::stream_array b(n, stream_bench::source_value);
        stream_bench::stream_array dst(n, stream_bench::zero_value);

        for (auto _ : state)
        {
            // Out-of-line call into the backend TU; nothing else is needed here.
            kernels::triad(Backend{}, dst.data(), a.data(), b.data(), stream_bench::kScalar, n);
        }

        // Two arrays read and one written.
        stream_bench::set_counters(state, n, 3);
    }
}

#define PSB_REGISTER_TRIAD(name)                                               \
    [[maybe_unused]] static benchmark::Benchmark *const registered_##name =    \
        benchmark::RegisterBenchmark("triad/" #name, bm_triad<backends::name>) \
            ->Apply(stream_bench::sizes);

PSB_FOR_EACH_BUILT_BACKEND(PSB_REGISTER_TRIAD)
