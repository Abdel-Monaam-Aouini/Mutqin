add_library(MutqinCore
    src/api/version.cpp
)
add_library(Mutqin::Core ALIAS MutqinCore)

target_include_directories(MutqinCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(MutqinCore PUBLIC cxx_std_17)
target_compile_definitions(MutqinCore PRIVATE
    MUTQIN_VERSION_STRING="${PROJECT_VERSION}"
)
