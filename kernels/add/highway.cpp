#include "kernels/add/add.hpp"

#include "hwy/highway.h"

// Static dispatch only: the target is whatever the compile flags enable.
HWY_BEFORE_NAMESPACE();
namespace kernels
{
    namespace hn = hwy::HWY_NAMESPACE;

    void add(backends::highway, double *RESTRICT_KW dst, const double *RESTRICT_KW a, const double *RESTRICT_KW b, std::size_t n)
    {
        const hn::ScalableTag<double> d;
        const std::size_t lanes = hn::Lanes(d);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            const auto a_simd = hn::LoadU(d, a + i);
            const auto b_simd = hn::LoadU(d, b + i);
            const auto dst_simd = hn::Add(a_simd, b_simd);
            hn::StoreU(dst_simd, d, dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = a[i] + b[i];
        }
    }
}
HWY_AFTER_NAMESPACE();
