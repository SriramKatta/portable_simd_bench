#include "kernels/triad/triad.hpp"
#include "tests/stream_test.hpp"

namespace
{
    template <typename Backend>
    bool check_backend(const char *name)
    {
        // The sources are small integers and the scalar is 3.0, so fused and unfused triad agree exactly.
        return stream_test::check<2>(
            "triad", name,
            [](double *dst, const std::array<const double *, 2> &src, std::size_t n)
            { kernels::triad(Backend{}, dst, src[0], src[1], 3.0, n); },
            [](const std::array<double, 2> &values)
            { return values[0] + 3.0 * values[1]; });
    }
}

#define PSB_CHECK_TRIAD(name) ok = check_backend<backends::name>(#name) && ok;

int main()
{
    bool ok = true;
    PSB_FOR_EACH_BUILT_BACKEND(PSB_CHECK_TRIAD)
    return ok ? 0 : 1;
}
