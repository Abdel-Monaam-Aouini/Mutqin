include(GoogleTest)
set(MUTQIN_DATA_DIR "${PROJECT_SOURCE_DIR}/data/quran")

add_executable(mutqin_cli_tests test_wav.cpp)
target_link_libraries(mutqin_cli_tests PRIVATE mutqin_cli_lib GTest::gtest_main)
gtest_discover_tests(mutqin_cli_tests)

# End to end: Python writes posteriors for a recitation with a skipped word, the CLI replays them.
if(MUTQIN_PYTHON AND EXISTS "${MUTQIN_DATA_DIR}/quran-uthmani.txt")
    set(MUTQIN_CLI_POSTERIORS "${CMAKE_CURRENT_BINARY_DIR}/skipped.f32")
    add_test(NAME CliReplay.MakePosteriors
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/prototype/make_posteriors.py"
                "${MUTQIN_CLI_POSTERIORS}" --surah 1 --ayahs 1-7 --scenario skip --data "${MUTQIN_DATA_DIR}")
    set_tests_properties(CliReplay.MakePosteriors PROPERTIES FIXTURES_SETUP cli_posteriors)

    add_test(NAME CliReplay.ReportsTheSkippedWord
        COMMAND mutqin-cli --surah 1 --ayah 1-7 --data "${MUTQIN_DATA_DIR}" --posteriors "${MUTQIN_CLI_POSTERIORS}")
    set_tests_properties(CliReplay.ReportsTheSkippedWord PROPERTIES
        FIXTURES_REQUIRED cli_posteriors
        PASS_REGULAR_EXPRESSION "skipped")
endif()
