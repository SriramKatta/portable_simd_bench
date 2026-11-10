#include "kernels/triad/triad.hpp"

namespace kernels
{
    void triad(backends::ompsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, double scalar, std::size_t n)
    {
#pragma omp simd
        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = a[i] + scalar * b[i];
        }
    }
}
