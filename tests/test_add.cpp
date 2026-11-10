#include "kernels/add/add.hpp"
#include "tests/stream_test.hpp"

namespace
{
    template <typename Backend>
    bool check_backend(const char *name)
    {
        return stream_test::check<2>(
            "add", name,
            [](double *dst, const std::array<const double *, 2> &src, std::size_t n)
            { kernels::add(Backend{}, dst, src[0], src[1], n); },
            [](const std::array<double, 2> &values)
            { return values[0] + values[1]; });
    }
}

#define PSB_CHECK_ADD(name) ok = check_backend<backends::name>(#name) && ok;

int main()
{
    bool ok = true;
    PSB_FOR_EACH_BUILT_BACKEND(PSB_CHECK_ADD)
    return ok ? 0 : 1;
}
