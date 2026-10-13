cmake_minimum_required(VERSION 3.16)

project(Mutqin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MUTQIN_BUILD_TESTS "Build MutqinCore tests (fetches GoogleTest)" ON)
option(MUTQIN_BUILD_BENCH "Build the micro-benchmark (core/bench)" OFF)
option(MUTQIN_WERROR "Treat compiler warnings as errors (used by CI)" OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(MUTQIN_WERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

add_subdirectory(core)
add_subdirectory(cli)

if(MUTQIN_BUILD_BENCH)
    add_executable(mutqin_bench core/bench/bench.cpp)
    target_include_directories(mutqin_bench PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
    target_link_libraries(mutqin_bench PRIVATE Mutqin::Core)
endif()

if(MUTQIN_BUILD_TESTS)
    include(CTest)
    include(FetchContent)
    # Test-only dependency: GoogleTest (BSD-3-Clause). Never linked into the library or CLI.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(core/tests)
    add_subdirectory(cli/tests)
endif()
