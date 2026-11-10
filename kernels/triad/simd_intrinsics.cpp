#include "kernels/triad/triad.hpp"

#include "kernels/simd_isa.hpp"

namespace kernels
{
    void triad(backends::simd_intrinsics, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, double scalar, std::size_t n)
    {
        const std::size_t lanes = simd_isa::lanes();
        const simd_isa::vec s = simd_isa::set1(scalar);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const simd_isa::vec a_simd = simd_isa::loadu(a + i);
            const simd_isa::vec b_simd = simd_isa::loadu(b + i);
            const simd_isa::vec dst_simd = simd_isa::fmadd(s, b_simd, a_simd);
            simd_isa::storeu(dst + i, dst_simd);
        }
        for (; i < n; ++i)
        {
            dst[i] = a[i] + scalar * b[i];
        }
    }
}
