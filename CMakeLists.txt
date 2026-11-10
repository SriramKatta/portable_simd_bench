cmake_minimum_required(VERSION 3.25)
project(portable_simd_bench LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep IPO/LTO off on purpose: LTO would merge the per-backend flags and inline
# the kernels into the benchmark loop, defeating the whole comparison.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# ISA flags applied to EVERY backend (space-separated string).
#  (a) GCC and Clang default to 256-bit vectors on AVX-512 server CPUs, so
#      without -mprefer-vector-width=512 the autovec/ompsimd backends would run
#      256-bit while the explicit backends run 512-bit.
#  (b) Highway's static AVX2 target also needs AES and PCLMUL, so
#      -march=x86-64-v3 leaves Highway at 128-bit SSSE3 while the others use
#      AVX2. Verified with GCC 15.2: -march=haswell and -march=broadwell
#      do not enable AES either (Highway stays at SSSE3), while
#      "-march=haswell -maes" or -march=skylake (or newer) give Highway AVX2.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(PSB_ARCH_FLAGS_DEFAULT "-march=native -mprefer-vector-width=512")
else()
    set(PSB_ARCH_FLAGS_DEFAULT "-mcpu=native")
endif()
set(PSB_ARCH_FLAGS "${PSB_ARCH_FLAGS_DEFAULT}" CACHE STRING "ISA flags applied to every backend")
separate_arguments(PSB_ARCH_FLAGS_LIST UNIX_COMMAND "${PSB_ARCH_FLAGS}")

option(PSB_BUILD_TESTS "Build the correctness tests" ON)
enable_testing()

include(cmake/CPM.cmake)
include(cmake/google_bench.cmake)
include(cmake/xsimd.cmake)
include(cmake/ghighway.cmake)
include(cmake/ispc.cmake)
include(cmake/pystencils.cmake)
# include(cmake/mdspan.cmake)  # not needed until a multi-dimensional kernel is added

# Warnings for our own targets only (applied via this list, never globally).
set(PSB_WARNING_FLAGS -Wall -Wextra)

include(cmake/export_asm.cmake)

add_subdirectory(kernels)
add_subdirectory(bench)
if(PSB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
