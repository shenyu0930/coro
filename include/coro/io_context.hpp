#pragma once

#include <coro/config/coro.hpp>
#include <coro/detail/thread_meta.hpp>
#include <coro/detail/io_context_meta.hpp>
#include <coro/detail/worker_meta.hpp>

namespace coro {

using config::cache_line_size;

class io_context final {
public:

private:
    alignas(cache_line_size) detail::worker_meta worker;
};

} // coro