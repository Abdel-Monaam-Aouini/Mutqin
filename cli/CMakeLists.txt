add_library(mutqin_cli_lib STATIC wav.cpp)
target_include_directories(mutqin_cli_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(mutqin-cli main.cpp)
target_link_libraries(mutqin-cli PRIVATE Mutqin::Core mutqin_cli_lib)
