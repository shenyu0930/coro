#pragma once

#include <cstdint>

namespace coro::config {

enum class level : uint8_t {
    debug,
    info,
    warning,
    error,
    fatal,
    no_log
};

inline constexpr level log_level = level::info;

inline constexpr bool is_log_d = log_level <= level::debug;
inline constexpr bool is_log_i = log_level <= level::info;
inline constexpr bool is_log_w = log_level <= level::warning;
inline constexpr bool is_log_e = log_level <= level::error;
inline constexpr bool is_log_f = log_level <= level::fatal;

}