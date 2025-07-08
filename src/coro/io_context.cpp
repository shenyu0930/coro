#include "coro/config/coro.hpp"
#include "coro/detail/io_context_meta.hpp"
#include "coro/log/log.hpp"
#include <chrono>
#include <coro/detail/thread_meta.hpp>
#include <coro/io_context.hpp>
#include <cstdint>
#include <exception>
#include <mutex>

namespace coro {

// init io_context
void io_context::init() {
    detail::this_thread.ctx = this;
    detail::this_thread.ctx_id = this->id_;

    this->worker_.init(config::default_io_uring_entries);
}

// deinit io_context
void io_context::deinit() noexcept {
    detail::this_thread.ctx = nullptr;
    detail::this_thread.ctx_id = static_cast<config::ctx_id_t>(-1);

    this->worker_.deinit();

    auto& meta = detail::io_context_meta;
    std::lock_guard<std::mutex> lg{meta.mtx};

    --meta.ready_count;
    --meta.create_count;

    log::info("io_context deinit, meta.create_count[%u]", meta.create_count);
}

// runs on host_thread_
void io_context::start() {
    host_thread_ = std::thread{[this]{
        this->init();

        auto& meta = detail::io_context_meta;
        {
            std::unique_lock<std::mutex> lock{meta.mtx};
            ++meta.ready_count;
            log::info("io_context[%u] ready, (%u/%u)", this->id_, meta.create_count, meta.ready_count);

            if(!meta.cv.wait_for(lock, std::chrono::seconds{1}, []{
                return meta.create_count == meta.ready_count;
            })) {
                log::error("io_context initialization timeout. there exists an "
                "io_context that has not been started.\n");
                std::terminate();
            }
        }

        meta.cv.notify_all();

        this->run();
    }};
}


// submit sqe to io_uring
void io_context::submit() {
    worker_.poll_submission();
}

// reap cqe in io_uring
void io_context::complete() {
    uint32_t handled_num = worker_.peek_uring() ? worker_.poll_completion() : 0;
    bool is_fast_path = worker_.requests_to_submit | worker_.has_task_ready() | handled_num;

    if(is_fast_path) [[likely]] {
        return;
    }

    log::info("do completion bad path.\n");

    const auto& meta = detail::io_context_meta;
    if(!worker_.peek_uring() 
        && (worker_.requests_to_reap > 0 || meta.ready_count > 1)) {
        log::info("block on worker.wait_uring().\n");
        worker_.wait_uring();
    }

    handled_num = worker_.poll_completion();

    bool is_not_over = handled_num | (meta.ready_count > 1) | worker_.requests_to_reap;
    if(!is_not_over) [[likely]] {
        will_stop_ = true;
    }
}

// resume coroutine
void io_context::work() {
    auto num = worker_.number_to_schedule();
    log::info("worker[%u] will run %u times...\n", id_, num);

    for(; num > 0; --num) {
        worker_.work_once();
        if constexpr (config::submission_threshold != -1U) {
            worker_.check_submission_threshold();
        }
    }
}

// runs on host_thread_
void io_context::run() {
    log::info("io_context[%u] runs on %lx\n", this->id_, static_cast<uintptr_t>(this->host_thread_.native_handle()));

    while(!will_stop_) [[likely]] {
        work();

        submit();

        complete();
    }

    log::info("io_context[%u] stopped\n", this->id_);

    deinit();
}

} // coro

