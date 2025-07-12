#include "coro/config/coro.hpp"
#include "coro/detail/task_info.hpp"
#include "coro/detail/thread_meta.hpp"
#include "coro/detail/user_data.hpp"
#include <coro/detail/worker_meta.hpp>
#include <coro/detail/io_context_meta.hpp>
#include <coro/log/log.hpp>
#include <coroutine>
#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

namespace coro {

class io_context;

namespace detail {

thread_local thread_meta this_thread;

// get a free sqe
struct io_uring_sqe* worker_meta::get_free_sqe() noexcept {
    log::info("worker[%u] get free sqe\n", this_thread.ctx_id);

    ++requests_to_reap;
    ++requests_to_submit;

    
    auto* sqe = io_uring_get_sqe(&ring);

    return sqe;
}

// 
// [[nodiscard]] bool is_ring_need_enter() noexcept;

// blocking wait cqe
void worker_meta::wait_uring() noexcept {
    [[maybe_unused]] struct io_uring_cqe** _= nullptr;
    io_uring_wait_cqe(&this->ring, _);
}

// non-blocking check if there is cqe 
bool worker_meta::peek_uring() noexcept {
    struct io_uring_cqe** cqe = nullptr;
    io_uring_peek_cqe(&this->ring, cqe);

    return cqe != nullptr;
}

// get a task/coroutine to resume
std::coroutine_handle<> worker_meta::schedule() noexcept {
    auto& cur = this->reap_cur;

    log::info("worker[%u] try scheduling\n", this->ctx_id);

    std::coroutine_handle<> chosen_coro = reap_swap[cur.head()];
    cur.pop();

    return chosen_coro;
}

// init worker_meta
// io_uring_entries: number of entries in io_uring
void worker_meta::init(unsigned int io_uring_entries) {
    this->ctx_id = this_thread.ctx_id;
    this_thread.worker = this;

    if(io_uring_queue_init(io_uring_entries, &ring, 0) < 0) {
        log::error("io uring queue init failed.");
    }
}

// deinit worker_meta
void worker_meta::deinit() noexcept {
    this_thread.worker = nullptr;
}

// add task
void worker_meta::co_spawn_unsafe(std::coroutine_handle<> handle) noexcept {
    log::info("worker[%u] co_spawn_unsafe coro[%lx]\n", ctx_id, handle.address());

    forward_task(handle);
}

// add task
void worker_meta::co_spawn_safe_msg_ring(std::coroutine_handle<> handle) noexcept {
    worker_meta& from = *this_thread.worker;

    log::info("coro[%lx] is pushing to worker[%u] from worker[%u]"
                "by co_spawn_safe_msg_ring().\n",
                handle.address(), ctx_id, from.ctx_id);

    auto* const sqe = from.get_free_sqe();
    auto user_data = reinterpret_cast<uint64_t>(handle.address());
    io_uring_prep_msg_ring(sqe, ring_fd, 0, user_data, 0);
    io_uring_sqe_set_data(sqe, (void*)reserved_user_data::nop);
}

// add task
void worker_meta::co_spawn_auto(std::coroutine_handle<> handle) noexcept {
    if (detail::this_thread.worker == this
        || io_context_meta.ready_count == 0) {
            this->co_spawn_unsafe(handle);
    } else {
        this->co_spawn_safe_msg_ring(handle);
    }
}

// resume a task
void worker_meta::work_once() {
    const auto coro = this->schedule();
 
    log::info("worker[%u] resume %lx\n", this->ctx_id, coro.address());

    coro.resume();

    log::info("worker[%u] work once finished\n", this->ctx_id);
}

// use io_uring_enter
void worker_meta::check_submission_threshold() noexcept {
    if constexpr (config::submission_threshold != -1U) {
        // limited submission count
        if (requests_to_submit >= config::submission_threshold) {
            [[maybe_unused]] int submitted = io_uring_submit_and_get_events(&this->ring);
            log::info("submmited[%d].", submitted);
            requests_to_submit = 0;
        }
    }
}

// poll submission entries
void worker_meta::poll_submission() noexcept {
    if (requests_to_submit) [[likely]] {
        bool will_wait = !has_task_ready();

        log::info("worker_meta::poll_submission(): before submit and wait.\n");

        [[maybe_unused]] int res = io_uring_submit_and_wait(&this->ring, will_wait);

        log::info("submitted[%d].\n", res);

        requests_to_submit = 0;

        log::info("worker_meta::poll_submission(): after submit_and_wait\n");
    }
}

// poll completion entries
uint32_t worker_meta::poll_completion() noexcept {
    struct io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;
    io_uring_for_each_cqe(&this->ring, head, cqe) {
        this->handle_cq_entry(cqe);
        count++;
    }

    return count;
}

// push task to reap_swap
void worker_meta::forward_task(std::coroutine_handle<> handle) noexcept {
    if(!handle) {
        log::error("forwarding an empty task.");
        return;
    }

    auto& cur = reap_cur;

    log::info(
        "worker[%u] forward_task[%lx] to [%u]\n", ctx_id, handle.address(), cur.tail()
    );

    reap_swap[cur.tail()] = handle;
    cur.push();
}

// handle a cq_entry
void worker_meta::handle_cq_entry(struct io_uring_cqe *cqe) noexcept {
    --requests_to_reap;

    log::info("ctx poll_completion found, remaining=%d\n");

    uint64_t user_data = cqe->user_data;
    const int32_t result = cqe->res;
    [[maybe_unused]] const uint32_t flags = cqe->flags;

    if (result < 0) {
        log::info("cqe reports error: user_data=%lx, result=%d, flags=%u\n"
                    "message: %s\n",
                    user_data, result, flags, strerror(-result));
    }

    io_uring_cqe_seen(&this->ring, cqe);

    if constexpr (uint64_t(detail::reserved_user_data::none) > 0) {
        if (user_data < uint64_t(detail::reserved_user_data::none)) [[unlikely]] {
            handle_reserved_user_data(user_data);
            return;
        }
    }

    user_data_type selector = user_data_type(uint8_t(user_data & 0b111));
    user_data &= raw_task_info_mask;
    task_info* __restrict__ const io_info = CO_CONTEXT_ASSUME_ALIGNED(alignof(task_info))(
        reinterpret_cast<task_info*>(user_data)
    );

    switch(selector) {
        [[likely]] case user_data_type::task_info_ptr:
            io_info->result = result;
            forward_task(io_info->handle);
            break;
        case user_data_type::coroutine_handle:
            forward_task(std::coroutine_handle<>::from_address(
                reinterpret_cast<void*>(user_data)
            ));
            break;
        case user_data_type::task_info_ptr_link_sqe:
            io_info->result = result;
            break;
        case user_data_type::msg_ring:
            forward_task(std::coroutine_handle<>::from_address(
                reinterpret_cast<void*>(user_data)
            ));
            ++requests_to_reap;
            break;
        [[unlikely]] case user_data_type::none:
            log::error("handle_cq_entry(): unknown case");
    }
}

// handle user data
void worker_meta::handle_reserved_user_data(uint64_t user_data) noexcept {
    switch (reserved_user_data(user_data)) {
        case reserved_user_data::nop:
            break;
        [[unlikely]] case reserved_user_data::none:
            break;
    }
}

// check if adequate sqes are inited
bool worker_meta::check_init(unsigned except_sqring_size) const noexcept {
    const unsigned actual_sqring_size = ring.cq.ring_mask + 1;

    if(actual_sqring_size < except_sqring_size) {
        log::error("worker_meta::check_init:"
                        "entries inside io_uring are not enough.\n"
                        "actual=%u, expect=%u",
                        actual_sqring_size, except_sqring_size);

        return false;
    }

    if(actual_sqring_size != except_sqring_size) {
        log::warn("worker_meta::init_check:"
                    "sqring_size mismatch: actual=%u, expect=%u\n",
                    actual_sqring_size, except_sqring_size);
    }

    return true;
}

} // detail

} // coro