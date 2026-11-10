#pragma once

#include <cstddef>

#if defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>
#elif defined(__ARM_NEON) && defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__AVX512F__) || defined(__AVX__) || defined(__SSE2__)
#include <immintrin.h>
#endif

// The vector ISA used by the simd_intrinsics backend, selected once for all kernels at
// compile time from whatever the compile flags enable: ARM SVE, else AArch64 NEON, else
// x86 AVX-512F, else AVX, else SSE2. Only kernels/<kernel>/simd_intrinsics.cpp includes
// this header. Every kernel is the same loop for all of them: full vectors, then a scalar
// remainder.
//
//   vec                      the vector type
//   lanes()                  doubles per vector; a run-time value with SVE, whose vector
//                            length is not known at compile time
//   loadu / storeu           unaligned load and store of one full vector
//   set1(x)                  x in every lane
//   add(a, b), mul(a, b)     lane-wise a + b and a * b
//   fmadd(a, b, c)           a * b + c; fused with SVE, NEON, AVX-512F, and with AVX when
//                            FMA is enabled (__FMA__), otherwise a separate multiply and add
namespace kernels::simd_isa
{
#if defined(__ARM_FEATURE_SVE)
    // Every operation uses the all-true predicate: full vectors only, the tail is left to
    // the scalar remainder loop like on the other ISAs.
    using vec = svfloat64_t;
    inline std::size_t lanes() { return svcntd(); }
    inline vec loadu(const double *p) { return svld1_f64(svptrue_b64(), p); }
    inline void storeu(double *p, vec v) { svst1_f64(svptrue_b64(), p, v); }
    inline vec set1(double x) { return svdup_n_f64(x); }
    inline vec add(vec a, vec b) { return svadd_f64_x(svptrue_b64(), a, b); }
    inline vec mul(vec a, vec b) { return svmul_f64_x(svptrue_b64(), a, b); }
    inline vec fmadd(vec a, vec b, vec c) { return svmla_f64_x(svptrue_b64(), c, a, b); } // c + a * b
#elif defined(__ARM_NEON) && defined(__aarch64__)
    // float64x2_t exists on AArch64 only.
    using vec = float64x2_t;
    constexpr std::size_t lanes() { return 2; }
    inline vec loadu(const double *p) { return vld1q_f64(p); }
    inline void storeu(double *p, vec v) { vst1q_f64(p, v); }
    inline vec set1(double x) { return vdupq_n_f64(x); }
    inline vec add(vec a, vec b) { return vaddq_f64(a, b); }
    inline vec mul(vec a, vec b) { return vmulq_f64(a, b); }
    inline vec fmadd(vec a, vec b, vec c) { return vfmaq_f64(c, a, b); } // c + a * b
#elif defined(__AVX512F__)
    using vec = __m512d;
    constexpr std::size_t lanes() { return 8; }
    inline vec loadu(const double *p) { return _mm512_loadu_pd(p); }
    inline void storeu(double *p, vec v) { _mm512_storeu_pd(p, v); }
    inline vec set1(double x) { return _mm512_set1_pd(x); }
    inline vec add(vec a, vec b) { return _mm512_add_pd(a, b); }
    inline vec mul(vec a, vec b) { return _mm512_mul_pd(a, b); }
    inline vec fmadd(vec a, vec b, vec c) { return _mm512_fmadd_pd(a, b, c); }
#elif defined(__AVX__)
    using vec = __m256d;
    constexpr std::size_t lanes() { return 4; }
    inline vec loadu(const double *p) { return _mm256_loadu_pd(p); }
    inline void storeu(double *p, vec v) { _mm256_storeu_pd(p, v); }
    inline vec set1(double x) { return _mm256_set1_pd(x); }
    inline vec add(vec a, vec b) { return _mm256_add_pd(a, b); }
    inline vec mul(vec a, vec b) { return _mm256_mul_pd(a, b); }
#if defined(__FMA__)
    inline vec fmadd(vec a, vec b, vec c) { return _mm256_fmadd_pd(a, b, c); }
#else
    inline vec fmadd(vec a, vec b, vec c) { return _mm256_add_pd(_mm256_mul_pd(a, b), c); }
#endif
#elif defined(__SSE2__)
    using vec = __m128d;
    constexpr std::size_t lanes() { return 2; }
    inline vec loadu(const double *p) { return _mm_loadu_pd(p); }
    inline void storeu(double *p, vec v) { _mm_storeu_pd(p, v); }
    inline vec set1(double x) { return _mm_set1_pd(x); }
    inline vec add(vec a, vec b) { return _mm_add_pd(a, b); }
    inline vec mul(vec a, vec b) { return _mm_mul_pd(a, b); }
    inline vec fmadd(vec a, vec b, vec c) { return _mm_add_pd(_mm_mul_pd(a, b), c); }
#else
#error "simd_intrinsics backend needs ARM SVE, AArch64 NEON, or x86 SSE2/AVX/AVX-512F enabled (e.g. -march=native)"
#endif
}
