#pragma once

#include <coro/config/log.hpp>

#include <cstdio>

namespace coro {

namespace detail {

    template<typename... Args>
    void log(const char* __restrict__ fmt, const Args&... args) {
        fprintf(stdout, fmt, args...);
    }

    template<typename... Args>
    void err(const char* __restrict__ fmt, const Args&... args) {
        fprintf(stderr, fmt, args...);
    }

} // namespace detail

namespace log {

    template<typename... Args>
    void debug(const char* __restrict__ fmt, const Args&... args) {
        if constexpr (config::log_level <= config::level::debug) {
            detail::log(fmt, args...);
        }
    }   

    template<typename... Args>
    void info(const char* __restrict__ fmt, const Args&... args) {
        if constexpr (config::log_level <= config::level::info) {
            detail::log(fmt, args...);
        }
    }

    template<typename... Args>
    void warn(const char* __restrict__ fmt, const Args&... args) {
        if constexpr (config::log_level <= config::level::warning) {
            detail::err(fmt, args...);
        }
    }
    
    template<typename... Args>
    void error(const char* __restrict__ fmt, const Args&... args) {
        if constexpr (config::log_level <= config::level::error) {
            detail::err(fmt, args...);
        }
    }

    template<typename... Args>
    void fatal(const char* __restrict__ fmt, const Args&... args) {
        if constexpr (config::log_level <= config::level::fatal) {
            detail::err(fmt, args...);
        }
    }
} // namespace log

} // namespace coro