#include "kernels/add/add.hpp"

namespace kernels
{
    void add(backends::ompsimd, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, std::size_t n)
    {
#pragma omp simd
        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = a[i] + b[i];
        }
    }
}
