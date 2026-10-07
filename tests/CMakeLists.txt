# Core self-tests: cmake -DANTSCOPEZ_BUILD_TESTS=ON, then ctest.
add_executable(tdrmath_test tdrmath_test.cpp)
target_link_libraries(tdrmath_test PRIVATE antscopez_core)
add_test(NAME tdrmath COMMAND tdrmath_test)
