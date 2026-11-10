#include "kernels/scale/scale.hpp"
#include "tests/stream_test.hpp"

namespace
{
    template <typename Backend>
    bool check_backend(const char *name)
    {
        return stream_test::check<1>(
            "scale", name,
            [](double *dst, const std::array<const double *, 1> &src, std::size_t n)
            { kernels::scale(Backend{}, dst, src[0], 3.0, n); },
            [](const std::array<double, 1> &values)
            { return 3.0 * values[0]; });
    }
}

#define PSB_CHECK_SCALE(name) ok = check_backend<backends::name>(#name) && ok;

int main()
{
    bool ok = true;
    PSB_FOR_EACH_BUILT_BACKEND(PSB_CHECK_SCALE)
    return ok ? 0 : 1;
}
