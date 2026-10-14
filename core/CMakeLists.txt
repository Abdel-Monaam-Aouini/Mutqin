add_library(MutqinCore
    src/align/aligner.cpp
    src/align/ctc.cpp
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
    find_path(SHERPA_INCLUDE_DIR sherpa-onnx/c-api/c-api.h HINTS "${SHERPA_ONNX_DIR}/include" REQUIRED)
    find_library(SHERPA_C_API NAMES sherpa-onnx-c-api HINTS "${SHERPA_ONNX_DIR}/lib" REQUIRED)
    target_include_directories(MutqinCore SYSTEM PRIVATE "${SHERPA_INCLUDE_DIR}")
    target_compile_definitions(MutqinCore PUBLIC MUTQIN_HAVE_SHERPA=1)
    target_link_libraries(MutqinCore PUBLIC "${SHERPA_C_API}")
    get_filename_component(SHERPA_LIB_DIR "${SHERPA_C_API}" DIRECTORY)
    set(MUTQIN_SHERPA_LIB_DIR "${SHERPA_LIB_DIR}" CACHE INTERNAL "")
    set_target_properties(MutqinCore PROPERTIES INTERFACE_LINK_DIRECTORIES "${SHERPA_LIB_DIR}")
endif()
