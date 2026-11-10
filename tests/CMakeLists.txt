add_executable(test_copy test_copy.cpp)
target_link_libraries(test_copy PRIVATE kernels)
target_compile_options(test_copy PRIVATE ${PSB_WARNING_FLAGS})
add_test(NAME copy COMMAND test_copy)
