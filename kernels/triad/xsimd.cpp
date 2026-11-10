#include "kernels/triad/triad.hpp"

#include <xsimd/xsimd.hpp>

namespace kernels
{
    void triad(backends::xsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, double scalar, std::size_t n)
    {
        // backends::xsimd shares its name with the library namespace, so qualify with ::.
        using batch = ::xsimd::batch<double>;
        constexpr std::size_t lanes = batch::size;
        const batch s(scalar);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const batch a_simd = batch::load_unaligned(a + i);
            const batch b_simd = batch::load_unaligned(b + i);
            const batch dst_simd = ::xsimd::fma(s, b_simd, a_simd); // s * b + a
            dst_simd.store_unaligned(dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = a[i] + scalar * b[i];
        }
    }
}
