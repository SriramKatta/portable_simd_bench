#include "kernels/copy/copy.hpp"

#include "kernels/simd_isa.hpp"

namespace kernels
{
    void copy(backends::simd_intrinsics, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n)
    {
        const std::size_t lanes = simd_isa::lanes();

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const simd_isa::vec src_simd = simd_isa::loadu(src + i);
            simd_isa::storeu(dst + i, src_simd);
        }
        for (; i < n; ++i)
        {
            dst[i] = src[i];
        }
    }
}
