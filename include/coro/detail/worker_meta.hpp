#pragma once

#include <coro/config/coro.hpp>
#include <coro/detail/thread_safe.hpp>
#include <coro/detail/spsc_cursor.hpp>
#include <liburing.h>

#include <array>
#include <coroutine>
#include <liburing/io_uring.h>

namespace coro {

class io_context;

namespace detail {

using config::cache_line_size;

struct worker_meta final {
    // io_uring
    alignas(cache_line_size) io_uring ring;

    // ring fd
    alignas(cache_line_size) int ring_fd;

    // io_context id 
    config::ctx_id_t ctx_id;

    // single-producer single-consumer queue
    alignas(cache_line_size) std::array<std::coroutine_handle<>, config::swap_capacity> reap_swap;
    spsc_cursor<config::cur_t, config::swap_capacity, unsafe> reap_cur;

    // number of I/O tasks running in io_uring
    int32_t requests_to_reap = 0;

    // number of I/O tasks need to be submitted to io_uring
    int32_t requests_to_submit = 0;

    // tasks ready need to be resumed
    [[nodiscard]] bool has_task_ready() const noexcept {
        return !reap_cur.is_empty();
    }

    // get a free sqe
    struct io_uring_sqe* get_free_sqe() noexcept;

    // 
    // [[nodiscard]] bool is_ring_need_enter() noexcept;

    // blocking wait cqe
    void wait_uring() noexcept;

    // non-blocking check if there is cqe 
    bool peek_uring() noexcept;

    // number of task waited for scheduling
    [[nodiscard]] config::cur_t number_to_schedule() const noexcept {
        const auto& cur = this->reap_cur;
        return cur.size();
    }

    // get a task/coroutine to resume
    [[nodiscard]] std::coroutine_handle<> schedule() noexcept;

    // init worker_meta
    // io_uring_entries: number of entries in io_uring
    void init(unsigned int io_uring_entries);

    // deinit worker_meta
    void deinit() noexcept;

    // add task
    void co_spawn_unsafe(std::coroutine_handle<> handle) noexcept;

    // add task
    void co_spawn_safe_msg_ring(std::coroutine_handle<> handle) noexcept;

    // add task
    void co_spawn_auto(std::coroutine_handle<> handle) noexcept;

    // resume a task
    void work_once();

    // use io_uring_enter
    void check_submission_threshold() noexcept;

    // poll submission entries
    void poll_submission() noexcept;

    // poll completion entries
    uint32_t poll_completion() noexcept;

    // push task to reap_swap
    void forward_task(std::coroutine_handle<> handle) noexcept;

    // handle a cq_entry
    void handle_cq_entry(struct io_uring_cqe *cqe) noexcept;

    // handle user data
    void handle_reserved_user_data(uint64_t user_data) noexcept;

    explicit worker_meta() noexcept = default;

    ~worker_meta() noexcept = default;

    // check if adequate sqes are inited
    [[nodiscard]] bool check_init(unsigned except_sqring_size) const noexcept;
};

}


} // coro