#pragma once

#include "coro/detail/thread_safe.hpp"
#include "coro/log/log.hpp"
#include <coro/config/coro.hpp>
#include <coro/detail/thread_meta.hpp>
#include <coro/detail/io_context_meta.hpp>
#include <coro/detail/worker_meta.hpp>
#include <coro/task.hpp>
#include <mutex>

namespace coro {

using config::cache_line_size;

class io_context final {
public:

private:
    alignas(cache_line_size) detail::worker_meta worker_;

    std::thread host_thread_;

    // current io_context id
    config::ctx_id_t id_;

    // stop io_context flag
    bool will_stop_ = false;

private:
    // init io_context
    void init();

    // deinit io_context
    void deinit();

    // runs on host_thread_
    void run();

    // add task
    friend void co_spawn(task<void>&& entrance) noexcept;

    // add task
    friend void co_spawn_unsafe(task<void>&& entrance) noexcept;

    // submit sqe to io_uring
    void submit();

    // reap cqe in io_uring
    void complete();

    // resume coroutine
    void work();

public: 
    explicit io_context() noexcept {
        auto& meta = detail::io_context_meta;
        std::lock_guard<std::mutex> lg{meta.mtx};

        this->id_ = meta.create_count++;

        log::info("&meta.create_count[%lx][%u]\n", &meta.create_count, meta.create_count);
    }

    void co_spawn(task<void>&& entrance) noexcept;

    template<safety is_thread_safe>
    void co_spawn(task<void>&& entrance) noexcept;

    // stop schedule
    void stop() noexcept {
        will_stop_ = true;
    }

    // runs on host_thread_
    void start();

    // join
    void join() {
        if(host_thread_.joinable()) {
            host_thread_.join();
        }
    }

    inline io_uring& ring() noexcept {
        return worker_.ring;
    }

    ~io_context() noexcept = default;

    // non-copyable
    // non-movable
    io_context(const io_context&) = delete;
    io_context(io_context&&) = delete;
    io_context& operator=(const io_context&) = delete;
    io_context& operator=(io_context&&) = delete;
};

inline void io_context::co_spawn(task<void>&& entrance) noexcept {
    this->co_spawn<safety::safe>(std::move(entrance));
}

template<safety is_thread_safe>
inline void io_context::co_spawn(task<void>&& entrance) noexcept {
    auto handle = entrance.get_handle();
    entrance.detach();

    if constexpr (is_thread_safe) {
        worker_.co_spawn_auto(handle);
    } else {
        worker_.co_spwan_unsafe(handle);
    }
}

inline void co_spawn(task<void>&& entrance) noexcept {
    auto handle = entrance.get_handle();
    entrance.detach();
    detail::this_thread.worker->co_spwan_unsafe(handle);
}

namespace detail {
    inline void co_spawn(std::coroutine_handle<> handle) noexcept {
        detail::this_thread.worker->co_spawn_auto(handle);
    }
}

inline void io_context_stop() noexcept {
    detail::this_thread.ctx->stop();
}

inline auto& this_io_context() noexcept {
    return *detail::this_thread.ctx;
}

} // coro