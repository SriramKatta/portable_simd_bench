#include "kernels/add/add.hpp"

#include "kernels/simd_isa.hpp"

namespace kernels
{
    void add(backends::simd_intrinsics, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, std::size_t n)
    {
        const std::size_t lanes = simd_isa::lanes();

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const simd_isa::vec a_simd = simd_isa::loadu(a + i);
            const simd_isa::vec b_simd = simd_isa::loadu(b + i);
            const simd_isa::vec dst_simd = simd_isa::add(a_simd, b_simd);
            simd_isa::storeu(dst + i, dst_simd);
        }
        for (; i < n; ++i)
        {
            dst[i] = a[i] + b[i];
        }
    }
}
