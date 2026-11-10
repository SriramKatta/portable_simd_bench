# One executable and one ctest test per kernel (tests/test_<kernel>.cpp);
# tests/stream_test.hpp is shared.
foreach(kernel IN LISTS PSB_KERNELS)
    add_executable(test_${kernel} test_${kernel}.cpp)
    target_link_libraries(test_${kernel} PRIVATE kernels)
    target_compile_options(test_${kernel} PRIVATE ${PSB_WARNING_FLAGS})
    add_test(NAME ${kernel} COMMAND test_${kernel})
endforeach()
