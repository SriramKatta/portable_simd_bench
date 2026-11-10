#pragma once

#include <cstddef>

#include "kernels/common.hpp"

// STREAM triad kernel: dst[i] = a[i] + scalar * b[i] over double.
//
// Contract for every overload: writes a[i] + scalar * b[i] to dst[i] for i in
// [0, n). No array may overlap any other. There is no alignment requirement on
// any pointer, n may be any value including 0, and nothing outside [0, n) is
// read or written. The multiply-add may be contracted into a fused
// multiply-add, so the result may differ from the unfused form in the last
// bit; every backend uses FMA where the instruction set has it, so that the
// backends are comparable.
//
// Each backend lives in its own translation unit (kernels/triad/<backend>.cpp)
// because each one is compiled with its own flags; the per-backend flags are
// set in kernels/CMakeLists.txt. The functions are therefore out-of-line and
// must never be defined inline in a header. Select a backend by passing its
// tag from kernels/common.hpp.
//
// The signature is written once, in PSB_DECLARE_TRIAD, and stamped out for every
// backend in the list in kernels/common.hpp.
#define PSB_DECLARE_TRIAD(backend) \
    void triad(backends::backend, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, double scalar, std::size_t n);

namespace kernels
{
    PSB_FOR_EACH_BACKEND(PSB_DECLARE_TRIAD)
}

#undef PSB_DECLARE_TRIAD
