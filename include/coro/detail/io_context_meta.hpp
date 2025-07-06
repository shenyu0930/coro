#pragma once

#include <coro/config/coro.hpp>

#include <condition_variable>
#include <mutex>

namespace coro {

namespace detail {
    struct io_context_meta_type {
        std::mutex mtx;
        std::condition_variable cv;
        config::ctx_id_t create_count;      // num of created io_context
        config::ctx_id_t ready_count;       // 
    };

    inline io_context_meta_type io_context_meta;
} // detail

} // coro