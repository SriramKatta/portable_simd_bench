#include "kernels/copy/copy.hpp"

#include "hwy/highway.h"

// Static dispatch only: the target is whatever the compile flags enable.
HWY_BEFORE_NAMESPACE();
namespace kernels
{
    namespace hn = hwy::HWY_NAMESPACE;

    void copy(backends::highway, double *RESTRICT_KW dst, const double *RESTRICT_KW src, std::size_t n)
    {
        const hn::ScalableTag<double> d;
        const std::size_t lanes = hn::Lanes(d);

        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            hn::StoreU(hn::LoadU(d, src + i), d, dst + i);
        }
        for (; i < n; ++i)
        {
            dst[i] = src[i];
        }
    }
}
HWY_AFTER_NAMESPACE();
