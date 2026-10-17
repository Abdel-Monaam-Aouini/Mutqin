set(MUTQIN_CORE_SOURCES
    src/align/aligner.cpp
    src/align/ctc.cpp
    src/align/locator.cpp
    src/api/engine.cpp
    src/api/version.cpp
    src/audio/resampler.cpp
    src/features/logmel.cpp
    src/inference/sherpa_recognizer.cpp
    src/inference/units.cpp
    src/vad/energy_vad.cpp
    src/text/quran.cpp
    src/text/sha256.cpp
    src/text/utf8.cpp
    src/tajweed/inventory.cpp
    src/tajweed/uthmani.cpp
    src/tajweed/phonemizer.cpp
)
add_library(MutqinCore ${MUTQIN_CORE_SOURCES})
add_library(Mutqin::Core ALIAS MutqinCore)

target_include_directories(MutqinCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(MutqinCore PRIVATE src)
target_compile_features(MutqinCore PUBLIC cxx_std_17)
target_compile_definitions(MutqinCore PRIVATE
    MUTQIN_VERSION_STRING="${PROJECT_VERSION}"
)

# Optional recognition backend: sherpa-onnx (Apache-2.0), streaming CTC models through its C API.
# Download a release from https://github.com/k2-fsa/sherpa-onnx/releases (the "shared" archive for your
# platform) and pass its folder: -DMUTQIN_WITH_SHERPA=ON -DSHERPA_ONNX_DIR=/path/to/sherpa-onnx-vX-...
option(MUTQIN_WITH_SHERPA "Build the sherpa-onnx recognition backend" OFF)
set(SHERPA_ONNX_DIR "" CACHE PATH "Extracted sherpa-onnx release (with include/ and lib/)")
if(MUTQIN_WITH_SHERPA)
    # NO_CMAKE_FIND_ROOT_PATH: cross-compiling toolchains (Android NDK) otherwise search only their own sysroot
    find_path(SHERPA_INCLUDE_DIR sherpa-onnx/c-api/c-api.h HINTS "${SHERPA_ONNX_DIR}/include" NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    find_library(SHERPA_C_API NAMES sherpa-onnx-c-api HINTS "${SHERPA_ONNX_DIR}/lib" NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    target_include_directories(MutqinCore SYSTEM PRIVATE "${SHERPA_INCLUDE_DIR}")
    target_compile_definitions(MutqinCore PUBLIC MUTQIN_HAVE_SHERPA=1)
    target_link_libraries(MutqinCore PUBLIC "${SHERPA_C_API}")
    get_filename_component(SHERPA_LIB_DIR "${SHERPA_C_API}" DIRECTORY)
    set(MUTQIN_SHERPA_LIB_DIR "${SHERPA_LIB_DIR}" CACHE INTERNAL "")
    set_target_properties(MutqinCore PROPERTIES INTERFACE_LINK_DIRECTORIES "${SHERPA_LIB_DIR}")
endif()

# Shared library for apps (Android .so, desktop): only the C API of mutqin.h is exported.
# -DMUTQIN_BUILD_SHARED=ON builds libmutqin.so / libmutqin.dylib / mutqin.dll next to the static library.
option(MUTQIN_BUILD_SHARED "Build the shared library libmutqin (C API only)" OFF)
if(MUTQIN_BUILD_SHARED)
    add_library(mutqin_shared SHARED ${MUTQIN_CORE_SOURCES})
    set_target_properties(mutqin_shared PROPERTIES
        OUTPUT_NAME mutqin
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    # export only mq_*: the C++ standard library templates in the objects must not leak out
    if(APPLE)
        target_link_options(mutqin_shared PRIVATE -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/mutqin.exp)
    elseif(UNIX)
        target_link_options(mutqin_shared PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/mutqin.map)
    endif()
    target_include_directories(mutqin_shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_include_directories(mutqin_shared PRIVATE src)
    target_compile_features(mutqin_shared PUBLIC cxx_std_17)
    target_compile_definitions(mutqin_shared PRIVATE MQ_BUILD_SHARED MUTQIN_VERSION_STRING="${PROJECT_VERSION}")
    target_compile_definitions(mutqin_shared INTERFACE MQ_USE_SHARED)
    if(MUTQIN_WITH_SHERPA)
        target_include_directories(mutqin_shared SYSTEM PRIVATE "${SHERPA_INCLUDE_DIR}")
        target_compile_definitions(mutqin_shared PRIVATE MUTQIN_HAVE_SHERPA=1)
        target_link_libraries(mutqin_shared PRIVATE "${SHERPA_C_API}")
    endif()
endif()

# Installation: headers, the static library, the shared library when built, and the command-line tool.
include(GNUInstallDirs)
install(FILES include/mutqin.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS MutqinCore ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(MUTQIN_BUILD_SHARED)
    install(TARGETS mutqin_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
