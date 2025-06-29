#pragma once

#include <coro/config/coro.hpp>

namespace coro {

class io_context;

namespace detail {

struct worker_meta;

struct alignas(config::cache_line_size) thread_meta {
    // running io_context
    io_context* ctx = nullptr;
    worker_meta* worker = nullptr;
    config::ctx_id_t ctx_id = static_cast<config::ctx_id_t>(-1);
};

extern thread_local thread_meta this_thread;

} // detail

} // coro