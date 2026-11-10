# One static library per backend (kernels_<backend>), each with its own flags.

# ---- Flag vocabulary, per compiler -----------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Disable the auto-vectorizer (loop and SLP) so only explicit SIMD remains.
    set(PSB_FLAGS_NO_AUTOVEC -fno-tree-vectorize)
    # Without this GCC replaces a copy loop by a call to memcpy, and we would
    # be benchmarking glibc, not the backend.
    set(PSB_FLAGS_NO_LIBCALL_IDIOMS -fno-tree-loop-distribute-patterns)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$")
    message(WARNING "The Clang flag set for the backends has not been verified "
                    "by disassembly yet; check the generated code before trusting results.")
    # Disable loop and SLP vectorizers.
    set(PSB_FLAGS_NO_AUTOVEC -fno-vectorize -fno-slp-vectorize)
    # Keep Clang from turning copy loops into libc calls.
    set(PSB_FLAGS_NO_LIBCALL_IDIOMS -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset)
else()
    message(FATAL_ERROR "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. The per-backend flag "
                        "policy (disable auto-vectorization, avoid libc call idioms) is only "
                        "defined for GNU and Clang; add a branch in kernels/CMakeLists.txt.")
endif()

# ---- OpenMP simd support ---------------------------------------------------
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fopenmp-simd PSB_HAVE_OPENMP_SIMD)
if(NOT PSB_HAVE_OPENMP_SIMD)
    message(FATAL_ERROR "The compiler does not support -fopenmp-simd, required by the ompsimd backend.")
endif()

# ---- Kernels and backends --------------------------------------------------
# Adding a kernel: add the directory kernels/<name>/<backend>.cpp files and list it here.
set(PSB_KERNELS copy)

add_library(kernels INTERFACE)

# psb_add_backend(<backend> [AUTOVEC] [NO_ASM_EXPORT] [SOURCE_BACKEND <name>]
#                 [COMPILE_OPTIONS ...] [LINK_LIBRARIES ...])
# The auto-vectorizer is off unless AUTOVEC is given, so that a new backend cannot
# pick up compiler-generated SIMD by forgetting a flag.
# SOURCE_BACKEND: compile kernels/<kernel>/<name>.cpp instead of <backend>.cpp, so two
# backends built with different flags can share one source (autovec uses reference.cpp).
# Every backend library gets the private definition PSB_BACKEND=<backend>, which a
# shared source uses to name its tag type (backends::PSB_BACKEND).
# NO_ASM_EXPORT: do not register the sources with psb_export_asm (the ispc wrapper
# is only a forwarding call; the kernel itself is not C++).
function(psb_add_backend backend)
    cmake_parse_arguments(ARG "AUTOVEC;NO_ASM_EXPORT" "SOURCE_BACKEND" "COMPILE_OPTIONS;LINK_LIBRARIES" ${ARGN})
    # Convention: kernels/<kernel>/<backend>.cpp, unless SOURCE_BACKEND names another file.
    set(source_backend ${backend})
    if(ARG_SOURCE_BACKEND)
        set(source_backend ${ARG_SOURCE_BACKEND})
    endif()
    list(TRANSFORM PSB_KERNELS APPEND /${source_backend}.cpp OUTPUT_VARIABLE sources)
    set(autovec_flags ${PSB_FLAGS_NO_AUTOVEC})
    if(ARG_AUTOVEC)
        set(autovec_flags "")
    endif()
    set(target kernels_${backend})
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR})
    target_compile_definitions(${target} PRIVATE PSB_BACKEND=${backend})
    target_compile_options(${target} PRIVATE
        ${PSB_WARNING_FLAGS}
        ${PSB_ARCH_FLAGS_LIST}
        ${PSB_FLAGS_NO_LIBCALL_IDIOMS}
        ${autovec_flags}
        ${ARG_COMPILE_OPTIONS})
    if(ARG_LINK_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${ARG_LINK_LIBRARIES})
    endif()
    if(NOT ARG_NO_ASM_EXPORT)
        if(ARG_SOURCE_BACKEND)
            psb_export_asm(${target} OUTPUT_NAME ${backend})
        else()
            psb_export_asm(${target})
        endif()
    endif()
    target_link_libraries(kernels INTERFACE ${target})
endfunction()

psb_add_backend(reference)
psb_add_backend(autovec         AUTOVEC SOURCE_BACKEND reference)
psb_add_backend(ompsimd         COMPILE_OPTIONS -fopenmp-simd)
psb_add_backend(simd_intrinsics)
psb_add_backend(xsimd           LINK_LIBRARIES xsimd)
psb_add_backend(highway         LINK_LIBRARIES hwy)

# ---- ISPC backend (optional) -----------------------------------------------
# Only when cmake/ispc.cmake found an ISPC compiler. The C++ wrapper
# (<kernel>/ispc.cpp) goes through psb_add_backend like every other backend; the
# .ispc sources are compiled by psb_target_ispc_sources (see cmake/ispc.cmake) into
# kernels_ispc, without any of the GCC flags.
if(PSB_HAVE_ISPC)
    message(WARNING "The ispc backend was written without an ISPC compiler available and has "
                    "never been built or run. Check kernels/copy/copy.ispc, the generated header "
                    "and the compile command before trusting it or its results.")

    psb_add_backend(ispc NO_ASM_EXPORT)

    # Convention: kernels/<kernel>/<kernel>.ispc.
    list(TRANSFORM PSB_KERNELS REPLACE "(.+)" "\\1/\\1.ispc" OUTPUT_VARIABLE PSB_ISPC_SOURCES)
    psb_target_ispc_sources(kernels_ispc SOURCES ${PSB_ISPC_SOURCES})
    target_compile_definitions(kernels INTERFACE PSB_HAVE_ISPC)
endif()

# ---- pystencils backend (optional) -----------------------------------------
# Only when cmake/pystencils.cmake found pystencils and pystencils-sfg. The C++ wrapper
# (<kernel>/pystencils.cpp) goes through psb_add_backend like every other backend; the
# generator scripts are run and their output compiled into kernels_pystencils by
# psb_target_pystencils_sources (see cmake/pystencils.cmake).
if(PSB_HAVE_PYSTENCILS)
    message(WARNING "The pystencils backend was written without pystencils available and has "
                    "never been generated or built. Check kernels/copy/copy_pystencils.py, the "
                    "generated header and the compile command before trusting it or its results.")

    # NO_ASM_EXPORT: the wrapper is only a forwarding call, and the generated sources are
    # not known to psb_export_asm yet.
    psb_add_backend(pystencils NO_ASM_EXPORT)

    # Convention: kernels/<kernel>/<kernel>_pystencils.py.
    list(TRANSFORM PSB_KERNELS REPLACE "(.+)" "\\1/\\1_pystencils.py" OUTPUT_VARIABLE PSB_PYSTENCILS_SCRIPTS)
    psb_target_pystencils_sources(kernels_pystencils SCRIPTS ${PSB_PYSTENCILS_SCRIPTS})
    target_compile_definitions(kernels INTERFACE PSB_HAVE_PYSTENCILS)
endif()
