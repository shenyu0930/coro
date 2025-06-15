#include <coro/log/log.hpp>

int main(int, char**) {

    coro::log::debug("debug[%d]\n", 1); 
    coro::log::info("info[%d]\n", 2); 
    coro::log::warn("warn[%d]\n", 3); 
    coro::log::error("error[%d]\n", 4); 
    coro::log::fatal("fatal[%d]\n", 5); 

    return 0;
}