add_library(mutqin_cli_lib STATIC wav.cpp)
target_include_directories(mutqin_cli_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(mutqin-cli main.cpp)
target_link_libraries(mutqin-cli PRIVATE Mutqin::Core mutqin_cli_lib)

# Live microphone input (miniaudio 0.11.25, public domain / MIT-0, single header, used by the CLI only).
# Downloaded at configure time with a pinned checksum; without network the CLI builds without --mic.
option(MUTQIN_CLI_MIC "Build microphone capture into mutqin-cli (downloads miniaudio)" ON)
set(MUTQIN_HAVE_MIC OFF)
if(MUTQIN_CLI_MIC)
    set(MA_DIR "${CMAKE_BINARY_DIR}/_deps/miniaudio")
    set(MA_HEADER "${MA_DIR}/miniaudio.h")
    if(NOT EXISTS "${MA_HEADER}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h" "${MA_HEADER}"
            EXPECTED_HASH SHA256=ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89
            STATUS MA_STATUS)
        list(GET MA_STATUS 0 MA_CODE)
        if(NOT MA_CODE EQUAL 0)
            file(REMOVE "${MA_HEADER}")
            message(WARNING "miniaudio download failed (${MA_STATUS}); building mutqin-cli without --mic")
        endif()
    endif()
    if(EXISTS "${MA_HEADER}")
        set(MUTQIN_HAVE_MIC ON)
    endif()
endif()

if(MUTQIN_HAVE_MIC)
    find_package(Threads REQUIRED)
    target_sources(mutqin-cli PRIVATE mic.cpp)
    target_include_directories(mutqin-cli SYSTEM PRIVATE "${MA_DIR}")
    target_compile_definitions(mutqin-cli PRIVATE MUTQIN_HAVE_MIC=1)
    target_link_libraries(mutqin-cli PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

install(TARGETS mutqin-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
