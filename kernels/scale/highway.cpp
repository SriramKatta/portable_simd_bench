#include "kernels/scale/scale.hpp"

#include "hwy/highway.h"

// Static dispatch only: the target is whatever the compile flags enable.
HWY_BEFORE_NAMESPACE();
namespace kernels
{
    namespace hn = hwy::HWY_NAMESPACE;

    void scale(backends::highway, double *RESTRICT_KW dst, const double *RESTRICT_KW src, double scalar, std::size_t n)
    {
        const hn::ScalableTag<double> d;
        const std::size_t lanes = hn::Lanes(d);
        const auto s = hn::Set(d, scalar);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const auto src_simd = hn::LoadU(d, src + i);
            const auto dst_simd = hn::Mul(s, src_simd);
            hn::StoreU(dst_simd, d, dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = scalar * src[i];
        }
    }
}
HWY_AFTER_NAMESPACE();
