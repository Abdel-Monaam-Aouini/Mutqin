include(GoogleTest)

add_executable(mutqin_tests
    test_version.cpp
)
target_link_libraries(mutqin_tests PRIVATE Mutqin::Core GTest::gtest_main)
gtest_discover_tests(mutqin_tests)
