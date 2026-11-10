#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace kernels
{
    // Compile-time unrolling: calls f(0), f(1), ..., f(n-1) in order. Each index is
    // passed as std::integral_constant<std::size_t, k>, so it is a constant inside f.
    //
    // Usage (manually unrolled SIMD main loop):
    //   for (; i + 4 * lanes <= n; i += 4 * lanes)
    //   {
    //       unroll<4>([&](auto k) { store(dst + i + k * lanes, load(src + i + k * lanes)); });
    //   }
    template <std::size_t n, typename F, std::size_t... i>
    constexpr void unroll(F &&f, std::index_sequence<i...> = {})
    {
        if constexpr (sizeof...(i) != n)
        {
            return unroll<n>(std::forward<F>(f), std::make_index_sequence<n>());
        }
        else
        {
            (f(std::integral_constant<std::size_t, i>()), ...);
        }
    }
}
