cmake_minimum_required(VERSION 3.15)
project(coroutine)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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