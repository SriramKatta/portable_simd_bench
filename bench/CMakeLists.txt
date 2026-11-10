add_executable(simd_bench main.cpp copy.cpp)
target_link_libraries(simd_bench PRIVATE kernels benchmark::benchmark)
target_compile_options(simd_bench PRIVATE ${PSB_WARNING_FLAGS})
target_compile_definitions(simd_bench PRIVATE
    PSB_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
    PSB_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    PSB_ARCH_FLAGS="${PSB_ARCH_FLAGS}")
