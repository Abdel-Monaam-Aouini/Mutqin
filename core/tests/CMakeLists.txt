include(GoogleTest)

set(MUTQIN_DATA_DIR "${PROJECT_SOURCE_DIR}/data/quran")

add_executable(mutqin_tests
    test_version.cpp
    test_sha256.cpp
    test_quran.cpp
)
target_include_directories(mutqin_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_compile_definitions(mutqin_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
target_link_libraries(mutqin_tests PRIVATE Mutqin::Core GTest::gtest_main)
gtest_discover_tests(mutqin_tests)

# C++ loader vs Python reference loader (needs data/quran/ and a Python interpreter).
find_program(MUTQIN_PYTHON NAMES python3 python HINTS "${PROJECT_SOURCE_DIR}/.venv/bin")
if(MUTQIN_PYTHON AND EXISTS "${MUTQIN_DATA_DIR}/checksums.txt")
    add_executable(mutqin_parity_tests test_parity.cpp)
    target_include_directories(mutqin_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_compile_definitions(mutqin_parity_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
    target_link_libraries(mutqin_parity_tests PRIVATE Mutqin::Core GTest::gtest_main)

    set(MUTQIN_PY_DUMP "${CMAKE_CURRENT_BINARY_DIR}/quran_py_dump.tsv")
    add_test(NAME QuranParity.PythonDump
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/quran_text/dump.py"
                "${MUTQIN_PY_DUMP}" "${MUTQIN_DATA_DIR}")
    set_tests_properties(QuranParity.PythonDump PROPERTIES FIXTURES_SETUP quran_py_dump)

    add_test(NAME QuranParity.CppEqualsPython COMMAND mutqin_parity_tests)
    set_tests_properties(QuranParity.CppEqualsPython PROPERTIES
        FIXTURES_REQUIRED quran_py_dump
        ENVIRONMENT "MUTQIN_PY_DUMP=${MUTQIN_PY_DUMP}")
else()
    message(STATUS "Quran parity test disabled (need data/quran/ and python)")
endif()
