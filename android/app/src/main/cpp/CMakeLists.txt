# Native part of the app: the Mutqin core (compiled from the repository's core/ directory) with the
# sherpa-onnx backend, and a thin JNI layer over the C API in core/include/mutqin.h.
cmake_minimum_required(VERSION 3.22)
project(MutqinAndroid VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# core/ and the sherpa-onnx files (android/scripts/fetch_sherpa.sh) relative to this file
get_filename_component(MUTQIN_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(SHERPA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sherpa")

set(MUTQIN_WITH_SHERPA ON CACHE BOOL "" FORCE)
set(SHERPA_INCLUDE_DIR "${SHERPA_DIR}/include" CACHE PATH "" FORCE)
set(SHERPA_C_API "${SHERPA_DIR}/jniLibs/${ANDROID_ABI}/libsherpa-onnx-c-api.so" CACHE FILEPATH "" FORCE)
if(NOT EXISTS "${SHERPA_C_API}")
    message(FATAL_ERROR "sherpa-onnx libraries missing: run android/scripts/fetch_sherpa.sh")
endif()

add_subdirectory("${MUTQIN_REPO}/core" mutqin-core)

add_library(mutqin_jni SHARED jni_bridge.cpp)
target_include_directories(mutqin_jni PRIVATE "${MUTQIN_REPO}/core/include")
target_link_libraries(mutqin_jni PRIVATE MutqinCore log)
# 16 KB page-size devices (Android 15+): align the segments
target_link_options(mutqin_jni PRIVATE -Wl,-z,max-page-size=16384)
