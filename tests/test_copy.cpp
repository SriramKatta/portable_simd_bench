#include "kernels/copy/copy.hpp"
#include "tests/stream_test.hpp"

namespace
{
    template <typename Backend>
    bool check_backend(const char *name)
    {
        return stream_test::check<1>(
            "copy", name,
            [](double *dst, const std::array<const double *, 1> &src, std::size_t n)
            { kernels::copy(Backend{}, dst, src[0], n); },
            [](const std::array<double, 1> &values)
            { return values[0]; });
    }
}

#define PSB_CHECK_COPY(name) ok = check_backend<backends::name>(#name) && ok;

int main()
{
    bool ok = true;
    PSB_FOR_EACH_BUILT_BACKEND(PSB_CHECK_COPY)
    return ok ? 0 : 1;
}
