cmake_minimum_required(VERSION 3.16)
project(cpp_web_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build tests" OFF)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")


add_library(cpp_web_server
    src/server/Server.cpp
    src/http/HttpParser.cpp
)

if (WIN32)
    target_sources(cpp_web_server PRIVATE src/platform/Socket_win.cpp)
else()
    target_sources(cpp_web_server PRIVATE src/platform/Socket_posix.cpp)
endif()

target_include_directories(cpp_web_server PUBLIC include)

if (WIN32)
    target_compile_definitions(cpp_web_server PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS WIN32_LEAN_AND_MEAN)
    target_link_libraries(cpp_web_server PRIVATE ws2_32)
endif()

if (BUILD_EXAMPLES)
    add_executable(hello examples/hello_world.cpp)
    target_link_libraries(hello PRIVATE cpp_web_server)
endif()

