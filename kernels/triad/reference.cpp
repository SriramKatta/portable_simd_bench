#include "kernels/triad/triad.hpp"

#ifndef PSB_BACKEND
#error "PSB_BACKEND is not defined; build through CMake (psb_add_backend sets it)."
#endif

namespace kernels
{
    // This plain loop is compiled twice: as `reference` (vectorizer off) and as
    // `autovec` (vectorizer on). CMake sets PSB_BACKEND to the backend being built.
    void triad(backends::PSB_BACKEND, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, double scalar, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = a[i] + scalar * b[i];
        }
    }
}
