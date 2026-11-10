#include "kernels/scale/scale.hpp"

#include "kernels/simd_isa.hpp"

namespace kernels
{
    void scale(backends::simd_intrinsics, double *RESTRICT_KW dst, const double *RESTRICT_KW src, double scalar, std::size_t n)
    {
        const std::size_t lanes = simd_isa::lanes();
        const simd_isa::vec s = simd_isa::set1(scalar);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const simd_isa::vec src_simd = simd_isa::loadu(src + i);
            const simd_isa::vec dst_simd = simd_isa::mul(s, src_simd);
            simd_isa::storeu(dst + i, dst_simd);
        }
        for (; i < n; ++i)
        {
            dst[i] = scalar * src[i];
        }
    }
}
