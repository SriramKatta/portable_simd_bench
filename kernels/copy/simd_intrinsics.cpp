#include "kernels/copy/copy.hpp"

#include <immintrin.h>

#if defined(__AVX512F__)
#define COPY_LANES 8
#define COPY_LOAD(p) _mm512_loadu_pd(p)
#define COPY_STORE(p, v) _mm512_storeu_pd((p), (v))
#elif defined(__AVX__)
#define COPY_LANES 4
#define COPY_LOAD(p) _mm256_loadu_pd(p)
#define COPY_STORE(p, v) _mm256_storeu_pd((p), (v))
#elif defined(__SSE2__)
#define COPY_LANES 2
#define COPY_LOAD(p) _mm_loadu_pd(p)
#define COPY_STORE(p, v) _mm_storeu_pd((p), (v))
#else
#error "simd_intrinsics backend needs x86 SSE2, AVX or AVX-512F enabled (e.g. -march=native)"
#endif

namespace kernels
{
    void copy(backends::simd_intrinsics, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n)
    {
        std::size_t i = 0;
        for (; i + COPY_LANES <= n; i += COPY_LANES)
        {
            COPY_STORE(dst + i, COPY_LOAD(src + i));
        }
        for (; i < n; ++i)
        {
            dst[i] = src[i];
        }
    }
}
