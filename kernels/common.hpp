#pragma once

#include <cstddef>

namespace backends
{
    struct refrenece
    {
    };

    struct autovec
    {
    };

    struct ompsimd
    {
    };

    struct xsimd
    {
    };

    struct highway
    {
    };

    struct ispc
    {
    };

    struct pystencils
    {
    };
}

#if defined(__INTEL_COMPILER)
#define RESTRICT_KW __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define RESTRICT_KW __restrict__
#else
#define RESTRICT_KW
#endif