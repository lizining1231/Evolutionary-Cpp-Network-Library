enable_testing()

add_executable(TCPserver_test
    test_buffer.cpp
    test_decoder.cpp
    test_connmgr.cpp
)

target_include_directories(TCPserver_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TCPserver_test
    im_gateway
    GTest::gtest_main
    GTest::gmock
    pthread
)

include(GoogleTest)
gtest_discover_tests(TCPserver_test)
