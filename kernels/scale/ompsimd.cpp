#include "kernels/scale/scale.hpp"

namespace kernels
{
    void scale(backends::ompsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW src, double scalar, std::size_t n)
    {
#pragma omp simd
        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = scalar * src[i];
        }
    }
}
