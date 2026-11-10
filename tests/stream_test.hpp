#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <vector>

// What the STREAM kernel tests (tests/test_<kernel>.cpp) have in common: the contract
// checks of kernels/<kernel>/<kernel>.hpp for one backend of one kernel.
namespace stream_test
{
    constexpr std::size_t kGuard = 8;
    constexpr double kSentinel = -12345.5;

    // Source values: small integers, different for every source array, so that the
    // result of every kernel (with scalar 3.0) is exactly representable and the fused
    // and the unfused form of triad give the same bits. They are also nonzero, so
    // that a kernel that skips or swaps an array is caught.
    inline double source_value(std::size_t array, std::size_t i)
    {
        return 1.0 + static_cast<double>((i * 7 + array * 13) % 61);
    }

    // Checks one backend of the kernel `kernel` over [0, n) for n in 0..130, 1001 and 4099,
    // every array (the destination and each of the Sources source arrays) independently at
    // offset 0 and offset 1 from the start of its allocation, with guard zones around dst.
    //
    //   call(dst, src, n)   runs the kernel; src is std::array<const double *, Sources>
    //   expected(values)    the value of dst[i], given the Sources source values at i
    //
    // Returns true on success; prints the first failure found, and PASS otherwise.
    template <std::size_t Sources, typename Call, typename Expected>
    bool check(const char *kernel, const char *name, Call call, Expected expected)
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
            // Bit 0 of `offsets` is the offset of dst, bit k + 1 the offset of source k.
            for (std::size_t offsets = 0; offsets < (std::size_t{1} << (Sources + 1)); ++offsets)
            {
                const std::size_t dst_offset = offsets & 1;

                // Layout of dst: [offset][guard][n elements][guard]; the offset shifts the
                // pointer away from the allocation start (unaligned case).
                std::vector<double> dst_storage(dst_offset + kGuard + n + kGuard, kSentinel);
                double *dst = dst_storage.data() + dst_offset + kGuard;

                std::array<std::vector<double>, Sources> src_storage;
                std::array<std::vector<double>, Sources> src_expected;
                std::array<const double *, Sources> src;
                for (std::size_t k = 0; k < Sources; ++k)
                {
                    const std::size_t offset = (offsets >> (k + 1)) & 1;
                    src_storage[k].resize(n + offset + 1);
                    for (std::size_t i = 0; i < src_storage[k].size(); ++i)
                    {
                        src_storage[k][i] = source_value(k, i);
                    }
                    src_expected[k] = src_storage[k];
                    src[k] = src_storage[k].data() + offset;
                }

                call(dst, src, n);

                for (std::size_t i = 0; i < n; ++i)
                {
                    std::array<double, Sources> values;
                    for (std::size_t k = 0; k < Sources; ++k)
                    {
                        values[k] = src[k][i];
                    }
                    if (dst[i] != expected(values))
                    {
                        std::printf("FAIL %s/%s: n=%zu offsets=%zu index=%zu: dst mismatch\n",
                                    kernel, name, n, offsets, i);
                        return false;
                    }
                }
                for (std::size_t g = 0; g < kGuard; ++g)
                {
                    if (dst_storage[dst_offset + g] != kSentinel)
                    {
                        std::printf("FAIL %s/%s: n=%zu offsets=%zu index=%zu: guard before dst overwritten\n",
                                    kernel, name, n, offsets, g);
                        return false;
                    }
                    if (dst[n + g] != kSentinel)
                    {
                        std::printf("FAIL %s/%s: n=%zu offsets=%zu index=%zu: guard after dst overwritten\n",
                                    kernel, name, n, offsets, n + g);
                        return false;
                    }
                }
                // Region before the guard (the offset element) must be untouched too.
                for (std::size_t g = 0; g < dst_offset; ++g)
                {
                    if (dst_storage[g] != kSentinel)
                    {
                        std::printf("FAIL %s/%s: n=%zu offsets=%zu index=%zu: dst allocation start overwritten\n",
                                    kernel, name, n, offsets, g);
                        return false;
                    }
                }
                for (std::size_t k = 0; k < Sources; ++k)
                {
                    if (src_storage[k] != src_expected[k])
                    {
                        std::printf("FAIL %s/%s: n=%zu offsets=%zu source=%zu: source modified\n",
                                    kernel, name, n, offsets, k);
                        return false;
                    }
                }
            }
        }
        std::printf("PASS %s/%s\n", kernel, name);
        return true;
    }
}
