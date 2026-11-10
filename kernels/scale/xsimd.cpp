#include "kernels/scale/scale.hpp"

#include <xsimd/xsimd.hpp>

namespace kernels
{
    void scale(backends::xsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW src, double scalar, std::size_t n)
    {
        // backends::xsimd shares its name with the library namespace, so qualify with ::.
        using batch = ::xsimd::batch<double>;
        constexpr std::size_t lanes = batch::size;
        const batch s(scalar);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const batch src_simd = batch::load_unaligned(src + i);
            const batch dst_simd = s * src_simd;
            dst_simd.store_unaligned(dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = scalar * src[i];
        }
    }
}
