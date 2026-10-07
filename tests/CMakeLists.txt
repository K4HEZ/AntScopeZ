# Core self-tests: cmake -DANTSCOPEZ_BUILD_TESTS=ON, then ctest.
add_executable(tdrmath_test tdrmath_test.cpp)
target_link_libraries(tdrmath_test PRIVATE antscopez_core)
add_test(NAME tdrmath COMMAND tdrmath_test)

# Offline look at a saved TDR scan: events under different thresholds.
add_executable(tdr_events_dump tdr_events_dump.cpp)
target_link_libraries(tdr_events_dump PRIVATE antscopez_core)
