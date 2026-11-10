#include "kernels/copy/copy.hpp"

namespace kernels
{
    void copy(backends::ompsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n)
    {
#pragma omp simd
        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = src[i];
        }
    }
}
