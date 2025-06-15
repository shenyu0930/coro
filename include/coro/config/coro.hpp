#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>

namespace coro::config {

// L1 Cache Size
#ifdef __cpp_lib_hardware_interference_size 
    inline constexpr size_t cache_line_size = 
        std::hardware_destructive_interference_size;
#else
    inline constexpr size_t cache_line_size = 64;
#endif

// io_context_meta : thread = 1 : n
// thread : worker = 1 : 1
// worker : coroutine = 1 : m
// max thread/io_context cnt
using ctx_id_t = uint8_t;

// inline constexpr cur_t swap_capacity = 8192;
// max task/coroutine size
using cur_t = uint32_t;
inline constexpr cur_t swap_capacity = 16384;

// max uring entry cnt
inline constexpr uint32_t default_io_uring_entries =
    std::bit_ceil<uint32_t>(swap_capacity * 2ULL);

// maximal batch submissions cnt, -1 means unlimited
inline constexpr uint32_t submission_threshold = -1U;

// semaphone
using semaphone_counting_t = std::ptrdiff_t;

// condition variable
using condition_variable_counting_t = std::uintptr_t;

// time correction
// timeout(time) => timeout(time + bias)
inline constexpr int64_t timeout_bias_nanosecond = -30'000;

} // namespace coro::config