cmake_minimum_required(VERSION 3.18)

project(coro)
set(libname "coro")

set(CMAKE_EXPORT_COMPLILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#    add_compile_options(-fcoroutines-ts) # 或者使用 -fcoroutines-ts，具体取决于你的Clang版本
#endif()

add_library("${libname}" INTERFACE)

target_sources(coro PRIVATE
    # 请替换为你的实际源文件
    # 可以在此处列出更多源文件
)

include_directories(
    include/
)

target_include_directories(
    ${libname}
    INTERFACE
    include/ 
)

target_link_libraries(
    ${libname}
    INTERFACE
    uring
)

add_executable(helloworld tests/helloworld.cpp)
# target_include_directories(helloworld PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_link_libraries(helloworld PRIVATE coro)

add_executable(generator tests/generator.cpp)
# target_include_directories(generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(generator PRIVATE coro)

add_executable(task tests/task.cpp tests/io_utils.cpp)
# target_include_directories(task PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(task PRIVATE coro)

add_executable(delay_and_print tests/delay_and_print.cpp)
target_include_directories(delay_and_print PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(delay_and_print PRIVATE coro)

add_executable(ping_pong tests/ping_pong.cpp)
target_include_directories(ping_pong PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ping_pong PRIVATE coro)