cmake_minimum_required(VERSION 3.15)
project(coroutine)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_library(LIBURING_PATH
    NAMES uring liburing.so.2 liburing.so.1
    PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/lib/x86_64-linux-gnu
    DOC "Path to liburing shared library"
)

message(STATUS "liburing path[${LIBURING_PATH}]")

add_compile_options(-g)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(helloworld ./test/helloworld.cpp)

add_custom_command(TARGET helloworld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Linking compile_commands.json for clangd"
)

add_executable(test_log ./test/test_log.cpp)
add_executable(test_liburing ./test/test_liburing.cpp)
target_link_libraries(test_liburing
    ${LIBURING_PATH}
)
add_executable(test_liburing_echo_server ./test/test_liburing_echo_server.cpp)
target_link_libraries(test_liburing_echo_server
    ${LIBURING_PATH}
)