#pragma once

#include <cstddef>

// The backend list, written once. X is a macro that takes a backend name.
//
// PSB_FOR_EACH_BACKEND        every backend: tags and kernel declarations exist
//                             for all of them, built or not.
// PSB_FOR_EACH_BUILT_BACKEND  only the backends that are part of this build; the
//                             optional ones depend on PSB_HAVE_<X>, which CMake
//                             defines for everything that links `kernels`.
//                             Benchmarks and tests iterate this one.
#define PSB_FOR_EACH_CORE_BACKEND(X) \
    X(reference)                     \
    X(autovec)                       \
    X(ompsimd)                       \
    X(simd_intrinsics)               \
    X(xsimd)                         \
    X(highway)

#define PSB_FOR_EACH_BACKEND(X)  \
    PSB_FOR_EACH_CORE_BACKEND(X) \
    X(ispc)                      \
    X(pystencils)

#ifdef PSB_HAVE_ISPC
#define PSB_IF_ISPC(X) X(ispc)
#else
#define PSB_IF_ISPC(X)
#endif
#ifdef PSB_HAVE_PYSTENCILS
#define PSB_IF_PYSTENCILS(X) X(pystencils)
#else
#define PSB_IF_PYSTENCILS(X)
#endif

#define PSB_FOR_EACH_BUILT_BACKEND(X) \
    PSB_FOR_EACH_CORE_BACKEND(X)      \
    PSB_IF_ISPC(X)                    \
    PSB_IF_PYSTENCILS(X)

// Tag types: select a backend by passing backends::<name>{}.
namespace backends
{
#define PSB_BACKEND_TAG(name) \
    struct name               \
    {                         \
    };
    PSB_FOR_EACH_BACKEND(PSB_BACKEND_TAG)
#undef PSB_BACKEND_TAG
}

#if defined(__INTEL_COMPILER)
#define RESTRICT_KW __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define RESTRICT_KW __restrict__
#else
#define RESTRICT_KW
#endif
