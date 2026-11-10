#include <cstddef>
#include <cstdio>
#include <vector>

#include "kernels/copy/copy.hpp"

namespace
{
    constexpr std::size_t kGuard = 8;
    constexpr double kSentinel = -12345.5;

    // Returns true on success; prints the first failure found.
    template <typename Backend>
    bool check(const char *name)
    {
        std::vector<std::size_t> sizes;
        for (std::size_t n = 0; n <= 130; ++n)
        {
            sizes.push_back(n);
        }
        sizes.push_back(1001);
        sizes.push_back(4099);

        for (std::size_t n : sizes)
        {
            for (std::size_t offset = 0; offset <= 1; ++offset)
            {
                // Layout: [offset][guard][n elements][guard]; offset shifts the
                // pointer away from the allocation start (unaligned case).
                std::vector<double> src_storage(n + offset + 1);
                std::vector<double> dst_storage(offset + kGuard + n + kGuard, kSentinel);
                std::vector<double> src_expected(n + offset + 1);
                for (std::size_t i = 0; i < src_storage.size(); ++i)
                {
                    src_storage[i] = 1.0 + static_cast<double>(i) * 3.0;
                    src_expected[i] = src_storage[i];
                }

                double *src = src_storage.data() + offset;
                double *dst = dst_storage.data() + offset + kGuard;
                kernels::copy(Backend{}, dst, src, n);

                for (std::size_t i = 0; i < n; ++i)
                {
                    if (dst[i] != src[i])
                    {
                        std::printf("FAIL copy/%s: n=%zu offset=%zu index=%zu: dst mismatch\n", name, n, offset, i);
                        return false;
                    }
                }
                for (std::size_t g = 0; g < kGuard; ++g)
                {
                    if (dst_storage[offset + g] != kSentinel)
                    {
                        std::printf("FAIL copy/%s: n=%zu offset=%zu index=%zu: guard before dst overwritten\n",
                                    name, n, offset, g);
                        return false;
                    }
                    if (dst[n + g] != kSentinel)
                    {
                        std::printf("FAIL copy/%s: n=%zu offset=%zu index=%zu: guard after dst overwritten\n",
                                    name, n, offset, n + g);
                        return false;
                    }
                }
                // Region before the guard (the offset element(s)) must be untouched too.
                for (std::size_t g = 0; g < offset; ++g)
                {
                    if (dst_storage[g] != kSentinel)
                    {
                        std::printf("FAIL copy/%s: n=%zu offset=%zu index=%zu: dst allocation start overwritten\n",
                                    name, n, offset, g);
                        return false;
                    }
                }
                for (std::size_t i = 0; i < src_storage.size(); ++i)
                {
                    if (src_storage[i] != src_expected[i])
                    {
                        std::printf("FAIL copy/%s: n=%zu offset=%zu index=%zu: src modified\n", name, n, offset, i);
                        return false;
                    }
                }
            }
        }
        std::printf("PASS copy/%s\n", name);
        return true;
    }
}

#define PSB_CHECK_COPY(name) ok = check<backends::name>(#name) && ok;

int main()
{
    bool ok = true;
    PSB_FOR_EACH_BUILT_BACKEND(PSB_CHECK_COPY)
    return ok ? 0 : 1;
}
