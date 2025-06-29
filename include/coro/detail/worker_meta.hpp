#pragma once

#include <coro/config/coro.hpp>
#include <coro/detail/thread_safe.hpp>
#include <coro/detail/spsc_cursor.hpp>
#include <liburing.h>

#include <array>
#include <coroutine>

namespace coro {

class io_context;

namespace detail {

using config::cache_line_size;

struct worker_meta final {
    alignas(cache_line_size) io_uring ring;
    alignas(cache_line_size) int ring_fd;
    config::ctx_id_t ctx_id;
    alignas(cache_line_size) std::array<std::coroutine_handle<>, config::swap_capacity> reap_swap;
    spsc_cursor<config::cur_t, config::swap_capacity, unsafe> reap_cur;
};

}


} // coro