#include "kernels/copy/copy.hpp"

#include <xsimd/xsimd.hpp>

namespace kernels
{
    void copy(backends::xsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n)
    {
        // backends::xsimd shares its name with the library namespace, so qualify with ::.
        using batch = ::xsimd::batch<double>;
        constexpr std::size_t lanes = batch::size;

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const batch src_simd = batch::load_unaligned(src + i);
            src_simd.store_unaligned(dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = src[i];
        }
    }
}
