# Optional ISPC backend: detection, and psb_target_ispc_sources() to compile ISPC
# sources into a target. Detection sets PSB_HAVE_ISPC; kernels/CMakeLists.txt calls the
# function only when it is set.
#
# ISPC is never downloaded. To point CMake at a compiler:
#   cmake -S . -B build -DCMAKE_ISPC_COMPILER=/path/to/ispc

option(PSB_ENABLE_ISPC "Build the ISPC backend if an ISPC compiler is found" ON)

# The ISPC --target, a single target (not a comma-separated list).
# For double kernels the gang width matters for comparability with the other
# backends: an -x16 target processes 16 doubles (two 512-bit registers) per
# iteration, an -x8 target one. Examples: avx512icl-x8 on Ice Lake (AVX-512,
# 8 doubles = one zmm register), avx2-i64x4 for AVX2 (4 doubles = one ymm).
set(PSB_ISPC_TARGET "host" CACHE STRING "ISPC --target for the ispc backend, e.g. avx512icl-x8 or avx2-i64x4")

set(PSB_HAVE_ISPC OFF)
if(PSB_ENABLE_ISPC)
    include(CheckLanguage)
    check_language(ISPC)
    if(CMAKE_ISPC_COMPILER)
        if(PSB_ISPC_TARGET MATCHES ",")
            message(FATAL_ERROR "PSB_ISPC_TARGET must be a single ISPC target, got '${PSB_ISPC_TARGET}'.")
        endif()
        enable_language(ISPC)
        set(PSB_HAVE_ISPC ON)
    else()
        message(STATUS "No ISPC compiler found: the ispc backend is skipped "
                       "(point CMake at one with -DCMAKE_ISPC_COMPILER=/path/to/ispc)")
    endif()
endif()

# Compile ISPC sources and add the resulting objects to an existing target.
#
#   psb_target_ispc_sources(<target> SOURCES <a.ispc>...
#                           [ISPC_TARGET <t>] [COMPILE_OPTIONS <opt>...])
#
# SOURCES          .ispc files; relative paths resolve against the calling
#                  directory's CMAKE_CURRENT_SOURCE_DIR, as for add_library.
# ISPC_TARGET      ISPC --target (default ${PSB_ISPC_TARGET}).
# COMPILE_OPTIONS  extra ISPC options, after the defaults.
#
# Creates the OBJECT library <target>_ispc_objects and adds its objects to <target>.
# For each source ISPC also writes a header <source stem>_ispc.h (CMake's default
# ISPC_HEADER_SUFFIX) into ${CMAKE_CURRENT_BINARY_DIR}/<target>_ispc_headers, which is a
# PRIVATE include directory of <target>: include it as "<stem>_ispc.h". All headers of a
# target share that directory, so the source stems must be unique within a target.
# Call it once per target (the object library name is derived from the target).
# It is an error when no ISPC compiler was found (PSB_HAVE_ISPC is false), so guard the
# call with if(PSB_HAVE_ISPC).
function(psb_target_ispc_sources target)
    cmake_parse_arguments(ARG "" "ISPC_TARGET" "SOURCES;COMPILE_OPTIONS" ${ARGN})
    if(NOT PSB_HAVE_ISPC)
        message(FATAL_ERROR "psb_target_ispc_sources: no ISPC compiler was found (PSB_HAVE_ISPC is false)")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "psb_target_ispc_sources: '${target}' is not a target")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "psb_target_ispc_sources(${target}): SOURCES is empty")
    endif()
    if(NOT ARG_ISPC_TARGET)
        set(ARG_ISPC_TARGET ${PSB_ISPC_TARGET})
    endif()

    # The ISPC compiler is only given the options below, so none of the GCC flags of
    # <target> (-march, -fno-tree-vectorize, -Wall, ...) ever reach it: the .ispc
    # sources live in their own ISPC-only OBJECT library.
    #
    # CMake documents ISPC_HEADER_DIRECTORY as a path relative to
    # CMAKE_CURRENT_BINARY_DIR, so the property gets the relative form and the
    # include path below gets the absolute one.
    set(objects ${target}_ispc_objects)
    set(header_subdir ${target}_ispc_headers)
    set(header_dir ${CMAKE_CURRENT_BINARY_DIR}/${header_subdir})

    add_library(${objects} OBJECT ${ARG_SOURCES})
    set_target_properties(${objects} PROPERTIES ISPC_HEADER_DIRECTORY ${header_subdir})
    # Optimisation level comes from CMAKE_ISPC_FLAGS_<CONFIG> (-O3 for Release).
    # --addressing=64: ISPC does addressing arithmetic in 32 bits by default, but
    # sizes are std::size_t and the chunk base in copy.ispc is a 64-bit offset.
    target_compile_options(${objects} PRIVATE
        --target=${ARG_ISPC_TARGET}
        --addressing=64
        ${ARG_COMPILE_OPTIONS})

    # Static libraries do not consume linked object libraries, so add the objects
    # as sources; add_dependencies makes sure the header exists before the C++
    # sources of <target> that include it are compiled.
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:${objects}>)
    add_dependencies(${target} ${objects})
    target_include_directories(${target} PRIVATE ${header_dir})
endfunction()
