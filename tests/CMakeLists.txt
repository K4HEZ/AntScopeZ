# Core self-tests: cmake -DANTSCOPEZ_BUILD_TESTS=ON, then ctest.
add_executable(tdrmath_test tdrmath_test.cpp)
target_link_libraries(tdrmath_test PRIVATE antscopez_core)
add_test(NAME tdrmath COMMAND tdrmath_test)

# Offline look at a saved TDR scan: events under different thresholds.
add_executable(tdr_events_dump tdr_events_dump.cpp)
target_link_libraries(tdr_events_dump PRIVATE antscopez_core)

# Page-aware print report (desktop/printreport.*) -> PDFs/PNGs for checking
# the layout without the GUI: print_report_demo outdir
find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
add_executable(print_report_demo print_report_demo.cpp
    ${CMAKE_SOURCE_DIR}/desktop/printreport.cpp
    ${CMAKE_SOURCE_DIR}/desktop/qcustomplot.cpp)
target_include_directories(print_report_demo PRIVATE ${CMAKE_SOURCE_DIR}/desktop)
target_link_libraries(print_report_demo PRIVATE Qt6::Widgets Qt6::PrintSupport)
