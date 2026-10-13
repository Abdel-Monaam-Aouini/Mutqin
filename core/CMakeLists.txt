add_library(MutqinCore
    src/align/aligner.cpp
    src/api/engine.cpp
    src/api/version.cpp
    src/features/logmel.cpp
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
