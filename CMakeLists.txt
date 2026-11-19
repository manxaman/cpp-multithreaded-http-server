cmake_minimum_required(VERSION 3.16)
project(multithreaded_http_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(server_core
    src/http.cpp
    src/router.cpp
    src/server.cpp
    src/thread_pool.cpp
)
target_include_directories(server_core PUBLIC include)
target_link_libraries(server_core PUBLIC Threads::Threads)
target_compile_options(server_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(http_server src/main.cpp)
target_link_libraries(http_server PRIVATE server_core)


include(CTest)

if(BUILD_TESTING)
    # Parser tests
    add_executable(parser_tests tests/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE server_core)
    add_test(NAME parser_tests COMMAND parser_tests)

    # Thread pool tests
    add_executable(thread_pool_tests tests/thread_pool_tests.cpp)
    target_link_libraries(thread_pool_tests PRIVATE server_core)
    add_test(NAME thread_pool_tests COMMAND thread_pool_tests)
endif()

