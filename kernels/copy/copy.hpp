#pragma once

#include <cstddef>

#include "kernels/common.hpp"

// STREAM copy kernel: dst[i] = src[i] over double.
//
// Contract for every overload: copies src[0..n) to dst[0..n). The ranges must
// not overlap. There is no alignment requirement on either pointer, n may be
// any value including 0, and nothing outside [0, n) is read or written.
//
// Each backend lives in its own translation unit (kernels/copy/<backend>.cpp)
// because each one is compiled with its own flags; the per-backend flags are
// set in kernels/CMakeLists.txt. The functions are therefore out-of-line and
// must never be defined inline in a header. Select a backend by passing its
// tag from kernels/common.hpp.
//
// The signature is written once, in PSB_DECLARE_COPY, and stamped out for every
// backend in the list in kernels/common.hpp.
#define PSB_DECLARE_COPY(backend) \
    void copy(backends::backend, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n);

namespace kernels
{
    PSB_FOR_EACH_BACKEND(PSB_DECLARE_COPY)
}

#undef PSB_DECLARE_COPY
