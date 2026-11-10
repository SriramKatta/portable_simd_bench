// STREAM copy kernel in ISPC: dst[i] = src[i] over double.
//
// Same contract as kernels::copy (kernels/copy/copy.hpp): n may be any value
// including 0, no alignment requirement, nothing outside [0, n) is touched.
//
// What this relies on (ISPC user guide, v1.32.0):
//  * The foreach index is a "const varying int32" ("Parallel Iteration
//    Statements"), so a single foreach cannot span more than 2^31 - 1
//    elements. n is a 64-bit quantity, therefore the range is walked in chunks
//    of at most 2^30 elements, each chunk being one foreach over an int32 count.
//  * The chunk base is a uniform uint64 and pointers are advanced by it. ISPC
//    does addressing arithmetic in 32 bits by default ("Selecting 32 or 64 Bit
//    Addressing"), so the build passes --addressing=64 (kernels/CMakeLists.txt).
//  * foreach handles a trip count that is not a multiple of the gang size
//    ("foreach naturally handles the case where programCount doesn't evenly
//    divide the number of elements"); the tail runs with a partial mask.
//    Whether the masked tail touches memory outside [0, n) is NOT stated in the
//    guide; it is checked by the guard zones in tests/test_copy.cpp.
//
// The exported name is a global C symbol, hence the non-generic name. The
// generated header (copy_ispc.h, namespace ispc) declares it for the C++ side.
export void copy_ispc(uniform double dst[], const uniform double src[], uniform uint64 n)
{
    // 2^30 elements: fits an int32 count with a wide margin.
    const uniform uint64 chunk = 1 << 30;

    uniform uint64 base = 0;
    while (base < n)
    {
        const uniform int32 count = (uniform int32)min(n - base, chunk);
        uniform double * uniform d = dst + base;
        const uniform double * uniform s = src + base;

        foreach (i = 0 ... count)
        {
            d[i] = s[i];
        }

        // base + count <= n, so this cannot overflow.
        base += count;
    }
}
