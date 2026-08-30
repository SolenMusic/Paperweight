add_executable(paperweight_tests
    test_main.cpp
)

configure_file(
    "${PROJECT_SOURCE_DIR}/examples/materials/default.pmat"
    "${CMAKE_CURRENT_BINARY_DIR}/default.pmat"
    COPYONLY
)

target_link_libraries(paperweight_tests PRIVATE Paperweight::Core)
target_compile_features(paperweight_tests PRIVATE cxx_std_20)
set_target_properties(paperweight_tests PROPERTIES CXX_EXTENSIONS OFF)
paperweight_enable_project_warnings(paperweight_tests)

add_test(NAME paperweight.core COMMAND paperweight_tests)
