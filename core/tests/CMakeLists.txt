include(GoogleTest)

# Where the tests read the Quran text; a cross-compiled run (Android) points it at a path on the device.
set(MUTQIN_TEST_DATA_DIR "${PROJECT_SOURCE_DIR}/data/quran" CACHE PATH "Quran text directory used by the tests")
set(MUTQIN_DATA_DIR "${MUTQIN_TEST_DATA_DIR}")

add_executable(mutqin_tests
    test_version.cpp
    test_sha256.cpp
    test_quran.cpp
    test_phonemizer.cpp
    test_api.cpp
    test_session_events.cpp
    test_session_window.cpp
    test_audio.cpp
    test_units.cpp
    test_locator.cpp
)
target_include_directories(mutqin_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_compile_definitions(mutqin_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
find_package(Threads REQUIRED)
target_link_libraries(mutqin_tests PRIVATE Mutqin::Core GTest::gtest_main Threads::Threads)
gtest_discover_tests(mutqin_tests)

# C++ loader vs Python reference loader (needs data/quran/ and a Python interpreter).
find_program(MUTQIN_PYTHON NAMES python3 python HINTS "${PROJECT_SOURCE_DIR}/.venv/bin")
if(MUTQIN_PYTHON AND EXISTS "${MUTQIN_DATA_DIR}/checksums.txt"
   AND EXISTS "${MUTQIN_DATA_DIR}/quran-uthmani.txt" AND EXISTS "${MUTQIN_DATA_DIR}/quran-simple-clean.txt")
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
    add_executable(mutqin_phonemizer_parity_tests test_phonemizer_parity.cpp)
    target_include_directories(mutqin_phonemizer_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_compile_definitions(mutqin_phonemizer_parity_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
    target_link_libraries(mutqin_phonemizer_parity_tests PRIVATE Mutqin::Core GTest::gtest_main)

    set(MUTQIN_PY_PHONEMIZER_DUMP "${CMAKE_CURRENT_BINARY_DIR}/phonemizer_py_dump.txt")
    add_test(NAME PhonemizerParity.PythonDump
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/phonemizer/dump.py"
                "${MUTQIN_PY_PHONEMIZER_DUMP}" "${MUTQIN_DATA_DIR}")
    set_tests_properties(PhonemizerParity.PythonDump PROPERTIES FIXTURES_SETUP phonemizer_py_dump)

    add_test(NAME PhonemizerParity.CppEqualsPython COMMAND mutqin_phonemizer_parity_tests)
    set_tests_properties(PhonemizerParity.CppEqualsPython PROPERTIES
        FIXTURES_REQUIRED phonemizer_py_dump
        ENVIRONMENT "MUTQIN_PY_PHONEMIZER_DUMP=${MUTQIN_PY_PHONEMIZER_DUMP}")
    add_executable(mutqin_aligner_parity_tests test_aligner_parity.cpp)
    target_include_directories(mutqin_aligner_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_link_libraries(mutqin_aligner_parity_tests PRIVATE Mutqin::Core GTest::gtest_main)

    set(MUTQIN_PY_ALIGN_DUMP "${CMAKE_CURRENT_BINARY_DIR}/aligner_py_dump.txt")
    add_test(NAME AlignerParity.PythonDump
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/prototype/dump_align.py"
                "${MUTQIN_PY_ALIGN_DUMP}" "${MUTQIN_DATA_DIR}")
    set_tests_properties(AlignerParity.PythonDump PROPERTIES FIXTURES_SETUP aligner_py_dump)

    add_test(NAME AlignerParity.CppEqualsPython COMMAND mutqin_aligner_parity_tests)
    set_tests_properties(AlignerParity.CppEqualsPython PROPERTIES
        FIXTURES_REQUIRED aligner_py_dump
        ENVIRONMENT "MUTQIN_PY_ALIGN_DUMP=${MUTQIN_PY_ALIGN_DUMP}")
    add_executable(mutqin_words_parity_tests test_words_parity.cpp)
    target_include_directories(mutqin_words_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_compile_definitions(mutqin_words_parity_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
    target_link_libraries(mutqin_words_parity_tests PRIVATE Mutqin::Core GTest::gtest_main)

    set(MUTQIN_PY_WORDS_DUMP "${CMAKE_CURRENT_BINARY_DIR}/words_py_dump.txt")
    add_test(NAME WordsParity.PythonDump
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/prototype/dump_words.py"
                "${MUTQIN_PY_WORDS_DUMP}" "${MUTQIN_DATA_DIR}")
    set_tests_properties(WordsParity.PythonDump PROPERTIES FIXTURES_SETUP words_py_dump)

    add_test(NAME WordsParity.CppEqualsPython COMMAND mutqin_words_parity_tests)
    set_tests_properties(WordsParity.CppEqualsPython PROPERTIES
        FIXTURES_REQUIRED words_py_dump
        ENVIRONMENT "MUTQIN_PY_WORDS_DUMP=${MUTQIN_PY_WORDS_DUMP}")
    add_executable(mutqin_features_parity_tests test_features_parity.cpp)
    target_include_directories(mutqin_features_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_link_libraries(mutqin_features_parity_tests PRIVATE Mutqin::Core GTest::gtest_main)

    set(MUTQIN_PY_FEATURES_DUMP "${CMAKE_CURRENT_BINARY_DIR}/features_py_dump.bin")
    add_test(NAME FeaturesParity.PythonDump
        COMMAND ${MUTQIN_PYTHON} -I "${PROJECT_SOURCE_DIR}/tools/train/dump_features.py" "${MUTQIN_PY_FEATURES_DUMP}")
    set_tests_properties(FeaturesParity.PythonDump PROPERTIES FIXTURES_SETUP features_py_dump)

    add_test(NAME FeaturesParity.CppEqualsNumpy COMMAND mutqin_features_parity_tests)
    set_tests_properties(FeaturesParity.CppEqualsNumpy PROPERTIES
        FIXTURES_REQUIRED features_py_dump
        ENVIRONMENT "MUTQIN_PY_FEATURES_DUMP=${MUTQIN_PY_FEATURES_DUMP}")
else()
    message(STATUS "Quran parity test disabled (need data/quran/ and python)")
endif()

# sherpa-onnx backend integration test (only when the backend is built in).
if(MUTQIN_WITH_SHERPA)
    add_executable(mutqin_sherpa_tests test_sherpa.cpp test_zipformer.cpp)
    target_include_directories(mutqin_sherpa_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_compile_definitions(mutqin_sherpa_tests PRIVATE MUTQIN_DATA_DIR="${MUTQIN_DATA_DIR}")
    target_link_libraries(mutqin_sherpa_tests PRIVATE Mutqin::Core mutqin_cli_lib GTest::gtest_main)
    set_target_properties(mutqin_sherpa_tests PROPERTIES BUILD_RPATH "${MUTQIN_SHERPA_LIB_DIR}")
    gtest_discover_tests(mutqin_sherpa_tests)
endif()

# The public header must be valid C and usable from C.
add_executable(mutqin_c_header_test test_c_header.c)
target_link_libraries(mutqin_c_header_test PRIVATE Mutqin::Core)
add_test(NAME CHeader.CompilesAndRunsAsC COMMAND mutqin_c_header_test)

# the number of functions the header declares (each starts a line with MQ_API)
file(STRINGS ${PROJECT_SOURCE_DIR}/core/include/mutqin.h mutqin_api_lines REGEX "^MQ_API ")
list(LENGTH mutqin_api_lines MUTQIN_API_FUNCTION_COUNT)

if(MUTQIN_BUILD_SHARED)
    add_executable(mutqin_c_header_test_shared test_c_header.c)
    target_link_libraries(mutqin_c_header_test_shared PRIVATE mutqin_shared)
    add_test(NAME CHeader.SharedLibraryWorksFromC COMMAND mutqin_c_header_test_shared)
    if(UNIX AND NOT APPLE)
        # only the C API may be exported by the shared library
        add_test(NAME CHeader.SharedLibraryExportsOnlyTheCApi
            COMMAND ${CMAKE_COMMAND} -DLIB=$<TARGET_FILE:mutqin_shared> -DEXPECTED=${MUTQIN_API_FUNCTION_COUNT}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exports.cmake)
    endif()
endif()
