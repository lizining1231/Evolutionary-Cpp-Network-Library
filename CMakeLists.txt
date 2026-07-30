cmake_minimum_required(VERSION 3.14)
project(cpp-traffic-gateway LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/.cache/googletest-src
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(im_gateway STATIC
    src/Buffer.cpp
    src/SocketListener.cpp
    src/Poller.cpp
    src/Connection.cpp
    src/ConnectionManager.cpp
    src/EventLoop.cpp
    src/decoder/DelimiterDecoder.cpp
    src/TimerQueue.cpp
    src/Config.cpp
)

target_include_directories(im_gateway PRIVATE
    src
    src/server
    include
)

add_executable(TCPserver
    src/main.cpp
)

target_include_directories(TCPserver PRIVATE
    src
    include
)

target_link_libraries(TCPserver im_gateway pthread)

add_subdirectory(test)

message(STATUS "Using C++${CMAKE_CXX_STANDARD} standard")
