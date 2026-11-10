# portable_simd_bench

Compares different ways of getting SIMD code for the same kernel: what the
compiler does on its own, what a pragma buys, hand-written intrinsics, and
portable SIMD libraries, all measured against a reference that contains no SIMD.

## Backends

| backend           | how the SIMD is produced                | status  |
|-------------------|-----------------------------------------|---------|
| `reference`       | none: scalar loop, vectorizer off       | done    |
| `autovec`         | compiler auto-vectorization (`reference.cpp`, vectorizer on) | done |
| `ompsimd`         | `#pragma omp simd`                      | done    |
| `simd_intrinsics` | raw intrinsics: x86 (AVX-512/AVX/SSE2), ARM (SVE/NEON) | x86 done; ARM written, never compiled (see below) |
| `xsimd`           | [xsimd](https://github.com/xtensor-stack/xsimd) 14.3.0 | done |
| `highway`         | [Google Highway](https://github.com/google/highway) 1.4.0, static dispatch | done |
| `ispc`            | [ISPC](https://github.com/ispc/ispc) compiler | written, never compiled (see below) |
| `pystencils`      | [pystencils](https://pycodegen.walberla.net) code generation via pystencils-sfg | written, never generated or compiled (see below) |

## Kernels

All kernels work on `double`. The last column is what `bytes_per_second` counts per
element (the STREAM convention: every array read or written counts once).

| kernel  | definition                                  | bytes per element |
|---------|---------------------------------------------|-------------------|
| `copy`  | STREAM copy, `dst[i] = src[i]`              | 16 |
| `scale` | STREAM scale, `dst[i] = scalar * src[i]`    | 16 |
| `add`   | STREAM add, `dst[i] = a[i] + b[i]`          | 24 |
| `triad` | STREAM triad, `dst[i] = a[i] + scalar * b[i]` | 24 |

No array may overlap another, there is no alignment requirement, `n` may be anything
including 0, and nothing outside `[0, n)` is touched. The multiply-add of `triad` may be
contracted into a fused multiply-add, so its result may differ from the unfused form in the
last bit; every backend uses FMA where the instruction set has it, so that they are comparable.

## Layout

```
CMakeLists.txt            options, dependencies, PSB_ARCH_FLAGS
cmake/                    one CPM file per third-party dependency; export_asm.cmake;
                          ispc.cmake and pystencils.cmake (optional backends)
kernels/
  common.hpp              backend tag types, RESTRICT_KW
  unroll.hpp              unroll<N>(f): compile-time unrolling for manually unrolled SIMD loops
  simd_isa.hpp            the vector ISA of simd_intrinsics (SVE, NEON, AVX-512F/AVX/SSE2), chosen once for all kernels
  pystencils_common.py    code shared by the pystencils generator scripts (--target, configuration)
  CMakeLists.txt          per-backend compiler flags (the flag policy lives here)
  copy/, scale/, add/, triad/   one directory per kernel, all laid out like this:
    copy.hpp              kernels::copy(backends::<tag>, dst, src, n), one overload per backend
    <backend>.cpp         one translation unit per backend (autovec has none: it compiles reference.cpp)
    copy.ispc             the kernel in ISPC; ispc.cpp forwards to it
    copy_pystencils.py    pystencils-sfg generator script; pystencils.cpp forwards to the generated code
bench/
  main.cpp                records compiler + flags in the benchmark context
  copy.cpp, scale.cpp, add.cpp, triad.cpp   Google Benchmark driver per kernel
  stream.hpp              what the drivers share: arrays with first touch, counters, the size range
  aligned_buffer.hpp      harness-owned aligned memory
tests/
  test_copy.cpp, test_scale.cpp, test_add.cpp, test_triad.cpp   every backend against the contract
  stream_test.hpp         the contract check they share: remainders, offsets, guard zones
```

Every backend is its own translation unit and its own static library
(`kernels_<backend>`), because each one needs different compiler flags. Kernels
are therefore always out-of-line calls. LTO is switched off for the same reason.

## Flag policy

All backends get `PSB_ARCH_FLAGS` and have loop-to-libcall conversion disabled.
Without the latter GCC turns the copy loop into a call to `memcpy`, and every
loop-based backend would measure glibc.

| backend           | extra flags (GCC)                      | consequence                          |
|-------------------|----------------------------------------|--------------------------------------|
| `reference`       | `-fno-tree-vectorize`                  | scalar code only                     |
| `autovec`         | none (compiles `reference.cpp`)        | the compiler vectorizes on its own   |
| `ompsimd`         | `-fopenmp-simd -fno-tree-vectorize`    | only the pragma vectorizes           |
| `simd_intrinsics` | `-fno-tree-vectorize`                  | SIMD only from the intrinsics        |
| `xsimd`           | `-fno-tree-vectorize`                  | SIMD only from the library           |
| `highway`         | `-fno-tree-vectorize`                  | SIMD only from the library           |

`PSB_ARCH_FLAGS` defaults to `-march=native -mprefer-vector-width=512` on x86-64.
Two things to know when changing it:

- GCC and Clang prefer 256-bit vectors on AVX-512 server CPUs. Without
  `-mprefer-vector-width=512`, `autovec` and `ompsimd` run 256-bit while the
  explicit backends run 512-bit.
- Highway's static AVX2 target also requires AES and PCLMUL. With
  `-march=x86-64-v3`, `-march=haswell` or `-march=broadwell` (GCC 15.2) Highway
  falls back to 128-bit SSSE3 while the others use AVX2. Use
  `-march=haswell -maes` or `-march=skylake` for an AVX2 comparison.

On AArch64 the default is `-mcpu=native` (no `-mprefer-vector-width`, which is x86 only); see "ARM" below.

Only GCC has been verified by disassembly. The Clang flag set in
`kernels/CMakeLists.txt` is untested and warns at configure time.

## Build and run

Needs CMake >= 3.25, a C++20 compiler, and network access on the first
configure (CPM fetches Google Benchmark, xsimd and Highway). Every new build
directory fetches them again unless a shared cache is set, e.g.
`export CPM_SOURCE_CACHE=$HOME/.cache/CPM`.

```sh
cmake -S . -B build
cmake --build build -j8
ctest --test-dir build --output-on-failure   # correctness of every backend
./build/bench/simd_bench                     # all backends, all sizes
```

In the `cusolvermp-srkatta-dev` container the default `g++` is 8.5, so enable
GCC 15 first:

```sh
docker exec -it cusolvermp-srkatta-dev bash
source /opt/rh/gcc-toolset-15/enable
cd ~/workspace/research_space/portable_simd_bench
```

Useful benchmark invocations:

```sh
# one kernel, selected backends, one size
./build/bench/simd_bench --benchmark_filter='copy/(reference|xsimd)/1024$'

# stable numbers: pin to a core, repeat, interleave, keep the medians
taskset -c 23 ./build/bench/simd_bench --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true --benchmark_enable_random_interleaving=true

# keep results together with the compiler and flags that produced them
./build/bench/simd_bench --benchmark_out=copy.json --benchmark_out_format=json
```

Benchmarks are named `<kernel>/<backend>/<n>`. Sizes run from 2^8 to 2^24
elements; the working set is `8 * n` bytes per array: `16 * n` for `copy` and
`scale`, `24 * n` for `add` and `triad`. `bytes_per_second` follows the STREAM
convention: every array read or written counts once (16 bytes per element for
`copy` and `scale`, 24 for `add` and `triad`), and the write-allocate traffic is not counted.

Unpinned single runs on a shared machine are not trustworthy: identical machine
code has shown differences of about 30% that way and none when pinned and repeated.

Other configurations:

```sh
cmake -S . -B build-avx2 -DPSB_ARCH_FLAGS="-march=skylake"
```

## Assembly export

Every backend's kernels can be exported as compiler-emitted assembly, compiled
with exactly the flags that backend is built with:

```sh
cmake --build build --target asm              # all backends, all kernels
cmake --build build --target asm_kernels_xsimd   # one backend
diff build/asm/copy/xsimd.s build/asm/copy/highway.s
```

Output goes to `build/asm/<kernel>/<backend>.s`, in the form OSACA and llvm-mca
take as input. Switches:

| variable         | default       | effect                                               |
|------------------|---------------|------------------------------------------------------|
| `PSB_EXPORT_ASM` | `OFF`         | `ON` makes the export part of every normal build     |
| `PSB_ASM_DIR`    | `<build>/asm` | where the files are written                          |
| `PSB_ASM_FLAGS`  | empty         | extra flags for the export only, e.g. `-fverbose-asm -masm=intel` |

`psb_export_asm(<target>)` in `cmake/export_asm.cmake` does the work and is
called from `psb_add_backend`, so new kernels and backends are exported without
further changes. It can be called on any other library target as well.

## ARM

No backend is x86-only: all of them follow `PSB_ARCH_FLAGS`, which defaults to `-mcpu=native` on
AArch64, the way they follow `-march=native` on x86. Configure prints a warning on AArch64
because none of this has ever been compiled (see Status).

| backend | what it uses on ARM |
|---|---|
| `reference`, `autovec`, `ompsimd` | whatever the compiler generates for the flags (`autovec` and `ompsimd` vectorize, `reference` does not) |
| `simd_intrinsics` | SVE intrinsics if the flags define `__ARM_FEATURE_SVE`, else NEON intrinsics if they define `__ARM_NEON` on AArch64, else the `#error` in `kernels/simd_isa.hpp` |
| `xsimd` | NEON64, or SVE only with a fixed vector length (see below) |
| `highway` | static dispatch: SVE when the flags enable it, else NEON (see below) |

`simd_intrinsics` on ARM:

- Both go through `kernels/simd_isa.hpp` like the x86 ISAs, so every kernel is one loop for
  all of them: full vectors, then a scalar remainder. Triad uses the fused `vfmaq_f64` (NEON)
  or `svmla_f64_x` (SVE).
- NEON is `float64x2_t`, 2 lanes. SVE is vector-length agnostic: the lane count is the
  run-time value `svcntd()`, and every operation uses the all-true predicate. The predicated
  tail that SVE allows is deliberately not used; the scalar remainder handles the tail as on
  every other ISA, and the benchmark sizes are multiples of any SVE vector length.
- SVE takes precedence over NEON when the flags enable both, which `-mcpu=native` does on an
  SVE machine. To measure NEON there, build with flags that do not enable SVE, e.g.
  `-DPSB_ARCH_FLAGS="-mcpu=native+nosve"` (untested). The other backends then use NEON too.

`xsimd` and `highway`, read from their sources (no source change was needed in the kernels;
nothing here was run on ARM):

- Highway 1.4.0 (`detect_targets.h`): `__ARM_FEATURE_SVE` makes `HWY_BASELINE_SVE` non-zero
  (`HWY_SVE`, or `HWY_SVE_256` with `__ARM_FEATURE_SVE_BITS == 256`), for GCC >= 10 and
  Clang >= 22, not Apple. The static target is the lowest set baseline bit and SVE has lower
  bits than NEON, so with SVE enabled `ScalableTag<double>` is scalable SVE; without it, NEON.
  Unverified: that this holds for the compiler you use.
- xsimd 14.3.0 (`xsimd_config.hpp`, `xsimd_sve_register.hpp`): `XSIMD_WITH_SVE` is 1 only when
  `__ARM_FEATURE_SVE_BITS` is also defined and nonzero, i.e. a fixed vector length given with
  `-msve-vector-bits=<n>` in `PSB_ARCH_FLAGS`; the SVE register is the fixed-size `sve<n>`.
  Without it, `XSIMD_WITH_NEON64` is set on AArch64 and `xsimd::batch<double>` should be a
  NEON64 batch (2 lanes) even when SVE is enabled. This is my reading of
  `xsimd_arch.hpp`'s best-architecture selection, not a test.

Status: the NEON and SVE paths were written with no ARM toolchain or headers. They were only
exercised against hand-written stand-ins for `<arm_neon.h>` and `<arm_sve.h>`, which checks
loop, tail and operand order but not the real intrinsic signatures. Before trusting
results: build and run `ctest` on ARM, and check the assembly for the expected vector width.

## ISPC backend

The `ispc` backend is optional and is built only when CMake finds an ISPC
compiler. Without one, configure prints a single status line and everything
else is unchanged.

```sh
cmake -S . -B build -DCMAKE_ISPC_COMPILER=/path/to/ispc -DPSB_ISPC_TARGET=avx512icl-x8
```

Pass `-DCMAKE_ISPC_COMPILER` explicitly: a build directory configured before
ISPC was installed has "not found" cached and will not look again.

`kernels/<kernel>/<kernel>.ispc` holds the kernel (exported as `<kernel>_ispc`);
`kernels/<kernel>/ispc.cpp` defines `kernels::<kernel>(backends::ispc, ...)` and forwards to
it through the header the ISPC compiler generates, so the benchmark and test drive it like
any other backend (`copy/ispc/<n>`). The `.ispc` sources are compiled by
`psb_target_ispc_sources(<target> SOURCES <a.ispc>... [ISPC_TARGET <t>] [COMPILE_OPTIONS <opt>...])`
in `cmake/ispc.cmake`, which can be used on any other library target as well
(once per target; the source file names must be unique).

`PSB_ISPC_TARGET` defaults to `host`. For `double` kernels pick the gang width
on purpose: an `-x16` target handles 16 doubles (two 512-bit registers) per
iteration, an `-x8` target one. `avx512icl-x8` matches the other backends on
Ice Lake, `avx2-i64x4` on AVX2.

Status: this backend was written from the ISPC user guide and CMake's ISPC
documentation with no ISPC compiler available. None of the `.ispc` files nor the
CMake branch that builds it has ever been compiled, and configure warns about
that when ISPC is found. Only the C++ wrapper and the benchmark/test
registration were exercised, against a stand-in for the ISPC function. Before
trusting results: run `ctest`, and check the generated code for `memcpy` calls
and the vector width. ISPC's own assembly is not part of the `asm` export yet.

## pystencils backend

The `pystencils` backend is optional and is built only when CMake finds a Python
(>= 3.12) that can import both `pystencils` and `pystencilssfg`. Without one,
configure prints a single status line and everything else is unchanged. CMake
never installs Python packages.

```sh
python3.12 -m venv /path/to/venv
/path/to/venv/bin/pip install pystencils pystencilssfg py-cpuinfo
cmake -S . -B build -DPystencilsSfg_PYTHON_INTERPRETER=/path/to/venv/bin/python
```

`kernels/<kernel>/<kernel>_pystencils.py` is a pystencils-sfg generator script; the
`--target` handling and the kernel configuration they share are in
`kernels/pystencils_common.py`. At build time each script emits
`<kernel>_pystencils.hpp/.cpp` with an explicitly vectorized kernel and the function
`psb_generated::<kernel>`, e.g. `psb_generated::copy(std::span<double>, std::span<const double>)`;
`scale` and `triad` take the `double` scalar as a last parameter after the spans.
The generated source is compiled inside `kernels_pystencils`, so it gets that
backend's flags. `kernels/<kernel>/pystencils.cpp` defines
`kernels::<kernel>(backends::pystencils, ...)` and forwards to it; a `static_assert`
there stops the build if the generated signature is not the expected one.
The generation is done by
`psb_target_pystencils_sources(<target> SCRIPTS <x.py>... [PYSTENCILS_TARGET <t>] [SCRIPT_ARGS <arg>...] [DEPENDS <file>...])`
in `cmake/pystencils.cmake`, which can be used on any other library target as
well (the script names must be unique). `DEPENDS` names further files the generation depends on;
`kernels/CMakeLists.txt` passes `pystencils_common.py`, so editing it regenerates every kernel.

`PSB_PYSTENCILS_TARGET` (default `CurrentCPU`) selects the pystencils SIMD
target: `X86_SSE`, `X86_AVX`, `X86_AVX512` or `ARM_NEON`. It has to match what
`PSB_ARCH_FLAGS` enables, because the generated intrinsics are compiled with
those flags. `CurrentCPU` needs `py-cpuinfo`; without it the script stops
instead of silently generating scalar code.

Status: written against the pystencils-sfg v0.2 documentation and the
pystencils 2.x documentation, with neither package installed. The scripts have
never been run, so the generated code and the CMake branch that builds it are
untested, and configure warns about that when the packages are found. The API
names used were checked against the sources of pystencils 2.1 and pystencils-sfg
0.3. Only the C++ wrapper and the benchmark/test registration were exercised,
against a stand-in for the generated function. The generated code is not part
of the `asm` export yet.

## Extending

Add a kernel `foo`:

1. `kernels/foo/foo.hpp`, with the signature written once as a `PSB_DECLARE_FOO(backend)` macro and
   stamped out with `PSB_FOR_EACH_BACKEND` (see `kernels/copy/copy.hpp`), and `kernels/foo/<backend>.cpp` for each (no `autovec.cpp`: `autovec` compiles `reference.cpp`).
   `simd_intrinsics.cpp` uses the vector type and operations of `kernels/simd_isa.hpp` for every ISA; add an operation there if the kernel needs one it lacks.
2. Append `foo` to `PSB_KERNELS` in the top-level `CMakeLists.txt`. The kernel libraries, the benchmark and
   the test are all built from that one list.
3. Optional: an ISPC version is `kernels/foo/foo.ispc` plus `kernels/foo/ispc.cpp`, a pystencils version is
   `kernels/foo/foo_pystencils.py` plus `kernels/foo/pystencils.cpp`; when those backends are enabled the
   build picks the files up automatically.
4. `bench/foo.cpp` and `tests/test_foo.cpp`.
   Build them on `bench/stream.hpp` (arrays, counters, size range) and `tests/stream_test.hpp` (the contract
   check, for kernels that compute `dst[i]` from `src[k][i]`); see `bench/triad.cpp` and `tests/test_triad.cpp`.
   A pystencils script for the kernel takes its `--target` handling and configuration from `kernels/pystencils_common.py`.

Add a backend `bar`:

1. One `X(bar)` entry in the backend list in `kernels/common.hpp`. This creates the tag and the
   declaration in every kernel header, and registers it in every benchmark and test.
2. `kernels/<kernel>/bar.cpp` for each kernel (or `SOURCE_BACKEND <name>` to compile another backend's file, as `autovec` does).
3. One `psb_add_backend(bar ...)` line in `kernels/CMakeLists.txt` with its flags and libraries.
   The auto-vectorizer is off by default; only a backend that wants it passes `AUTOVEC`.
