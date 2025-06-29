#pragma once

#include <coro/detail/thread_safe.hpp>
#include <coro/utility/as_atomic.hpp>

#include <bit>
#include <concepts>
#include <atomic>

namespace coro {

// single-producer single-consumer ring buffer
template <
    std::unsigned_integral T,
    T capacity,
    safety is_thread_safe = safety::safe,
    bool is_blocking = is_thread_safe
>
struct spsc_cursor {
    static_assert(std::has_single_bit(capacity));
    static_assert(!static_cast<bool>(is_thread_safe) || std::atomic<T>::is_always_lock_free);
    static_assert(
        static_cast<bool>(is_thread_safe) || !is_blocking
    );

    inline static constexpr T mask = capacity - 1;

    T _head = 0;
    T _tail = 0;

    [[nodiscard]] inline T head() const noexcept {
        return _head & mask;
    }

    [[nodiscard]] inline T tail() const noexcept {
        return _tail & mask;
    }

    [[nodiscard]] inline T raw_head() const noexcept {
        return _head;
    }

    [[nodiscard]] inline T raw_tail() const noexcept {
        return _tail;
    }

    [[nodiscard]] inline bool is_empty() const noexcept {
        return _head == _tail;
    }

    [[nodiscard]] inline T size() const noexcept {
        return _tail - _head;
    }

    [[nodiscard]] inline T available_number() const noexcept {
        return capacity - (_tail - _head);
    }

    [[nodiscard]] inline bool is_available() const noexcept {
        return available_number() > 0;
    }

    [[nodiscard]] inline T load_head() const noexcept {
        if constexpr (is_thread_safe) {
            return as_const_atomic(_head);
        }

        return head();
    }

    [[nodiscard]] inline T load_tail() const noexcept {
        if constexpr (is_thread_safe) {
            return as_const_atomic(_tail);
        }
        
        return tail();
    }

    [[nodiscard]] inline T load_raw_head() const noexcept {
        if constexpr (is_thread_safe) {
            return as_const_atomic(_head).load(std::memory_order_acquire);
        }

        return raw_head();
    }

    [[nodiscard]] inline T load_raw_tail() const noexcept {
        if constexpr (is_thread_safe) {
            return as_const_atomic(_tail).load(std::memory_order_acquire);
        }

        return raw_tail();
    }

    [[nodiscard]] inline T load_raw_tail_relaxed() const noexcept {
        if constexpr (is_thread_safe) {
            return as_const_atomic(_tail).load(std::memory_order_relaxed);
        }

        return raw_tail();
    }

    inline void store_raw_tail(T tail) noexcept {
        if constexpr (is_thread_safe) {
            as_atomic(_tail).store(tail, std::memory_order_release);
        }

        _tail = tail;
    }

    [[nodiscard]] inline bool is_empty_load_head() const noexcept {
        return _tail == load_raw_head();
    }

    [[nodiscard]] inline bool is_empty_load_tail() const noexcept {
        return _head == load_raw_tail();
    }

    [[nodiscard]] inline bool is_empty_load_tail_relaxed() const noexcept {
        return _head == load_raw_tail_relaxed();
    }

    [[nodiscard]] inline bool is_avaible_load_head() const noexcept {
        return capacity - (_tail - load_head()) > 0;
    }

    inline void wait_for_available() const noexcept {
        const T head_full = _tail - capacity;
        if constexpr (!is_thread_safe) {
            assert(head_full != load_raw_head());
            return;
        }

        if constexpr (is_blocking) {
            as_const_atomic(_head).wait(head_full, std::memory_order_acquire);
        } else {
            while(head_full == load_raw_head()) {}
        }
    }

    inline void wait_for_not_empty() const noexcept {
        const T tail_empty = _head;
        if constexpr (!is_thread_safe) {
            assert(tail_empty != load_raw_tail());
            return;
        }

        if constexpr (is_blocking) {
            as_const_atomic(_tail).wait(tail_empty, std::memory_order_acquire);
        } else {
            while(tail_empty == load_raw_tail()) {}
        }
    }

    inline void push(T num = 1) noexcept {
        if constexpr (is_thread_safe) {
            as_atomic(_tail).store(_tail + num, std::memory_order_release);
        } else {
            _tail += num;
        }
    }

    inline void pop(T num = 1) noexcept {
        if constexpr (is_thread_safe) {
            as_atomic(_head).store(_head + num, std::memory_order_release);
        } else {
            _head += num;
        }
    }

    inline void push_notify(T num = 1) noexcept {
        push(num);
        if constexpr (is_thread_safe && is_blocking) {
            as_atomic(_tail).notify_one();
        }
    }   

    inline void pop_notify(T num = 1) noexcept {
        pop(num);
        if constexpr (is_thread_safe && is_blocking) {
            as_atomic(_head).notify_one();
        }
    }
};

} // coro