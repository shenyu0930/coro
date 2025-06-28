#pragma once

#include <coro/detail/type_traits.hpp>

#include <cassert>
#include <concepts>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>

namespace coro {
    template<typename T>
    class task;

namespace detail {
    template <typename T>
    class task_promise_base;

    template <typename T>
    struct task_final_awaiter {
        static constexpr bool await_ready() noexcept {
            return false;
	    }

        template<typename Promise>
        requires std::derived_from<Promise, task_promise_base<T>>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> current) noexcept {
            // return caller
            return current.promise().caller;
        }

        constexpr void await_resume() const noexcept {}
    };
	
    template <>
    struct task_final_awaiter<void> {
        static constexpr bool await_ready() {
                return false;
        }
        
        template<typename Promise>
        requires std::derived_from<Promise, task_promise_base<void>>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> current) noexcept {
            // return caller
            auto& promise = current.promise();
            std::coroutine_handle<> caller = promise.caller;

            if(promise.is_detached) {
                // detached
                current.destroy();
            }

            return caller;
        }

        constexpr void await_resume() const noexcept {}
    };

    template <typename T>
    class task_promise_base {
    public:
        task_promise_base() noexcept = default;
        
        inline constexpr auto initial_suspend() noexcept {
            return std::suspend_always{};
        }
        
        inline constexpr task_final_awaiter<T> final_suspend() noexcept {
            // return caller task when finished
            return {};
        }

        inline void set_caller(std::coroutine_handle<> caller) {
            caller_ = caller;
        }

        task_promise_base(const task_promise_base&) = delete;
        task_promise_base(task_promise_base&&) = delete;
        task_promise_base operator=(const task_promise_base&) = delete;
        task_promise_base operator=(task_promise_base&&) = delete;
    private:
        std::coroutine_handle<> caller_{std::noop_coroutine()};
    };
    
    template <typename T>
    class task_promise final : public task_promise_base<T> {
        friend struct task_final_awaiter<void>;
        friend class task<void>;
    public:
        void unhandled_exception() {
            result_.emplace<2>(std::current_exception());
        }

        task<T> get_return_object() noexcept;

        template<typename V>
        requires std::convertible_to<V&&, T>
        void return_value(T&& result) noexcept(std::is_nothrow_convertible_v<V&&, T>) {
            result_.emplace<1>(std::forward<T>(result));
        }

        T& result() & {
            if(result_.index() == 2) {
                std::rethrow_exception(std::get<2>(result_));
            }

            assert(result_.index() == 1);
            return std::get<1>(result_);
        }

        T&& result() && {
            if(result_.index() == 2) {
                std::rethrow_exception(std::get<2>(result_));
            }

            assert(result_.index() == 1);
            return std::get<1>(result_);
        }
    private:
        std::variant<
            std::monostate,
            T,                  // return value
            std::exception_ptr  // exception
            > result_;
    };

    template <>
    class task_promise<void> final : public task_promise_base<void> {
    public:
        void unhandled_exception() {
            if(is_detached) {
                std::rethrow_exception(std::current_exception());
            } else {
                result_.emplace<2>(std::current_exception());
            }
        }

        constexpr void return_void() {}

        task<void> get_return_object() noexcept;

        void result() const {
            if(result_.index() == 2) [[unlikely]] {
                std::rethrow_exception(std::get<2>(result_));
            }
        }

    private:
        bool is_detached{false};
        std::variant<
            std::monostate,
            std::monostate,     // void
            std::exception_ptr  // exception
            > result_;
    };

    template<typename T>
    class task_promise<T &> final : public task_promise_base<T &> {
    public:
        task_promise() noexcept = default;

        void unhandled_exception() {
            result_.emplace<2>(std::current_exception());
        }

        task<T &> get_return_object() noexcept;

        void return_value(T& result) {
            result_.emplace<1>(std::addressof(result));
        }

        T& result() {
            if(result_.index() == 2) [[unlikely]] {
                std::rethrow_exception(std::get<2>(result_));
            }

            return *(std::get<1>(result_));
        }
    private:
        std::variant<
            std::monostate,
            T*,
            std::exception_ptr
        > result_;
    };
} // detail

    template<typename T = void>
    class task {
    public:
        using promise_type = detail::task_promise<T>;
        using value_type = T;
    private:
        struct awaiter_base {
            std::coroutine_handle<promise_type> handle;

            [[nodiscard]] inline bool await_ready() const noexcept {
                return !handle || handle.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coro) noexcept {
                handle.promise().set_caller(awaiting_coro);
                return handle;
            }
        };
    public:
        task() noexcept = default;

        explicit task(std::coroutine_handle<promise_type> current) noexcept
            : handle_(current) {}

        task(task&& other) noexcept : handle_(other.handle_) {
            other.handle = nullptr;
        }
        
        task(const task&) = delete;
        task &operator=(const task&) = delete;

        task &operator=(task&& other) noexcept {
            if(this != std::addressof(other)) {
                if(handle_) {
                    handle_.destroy();
                }

                handle_ = other.handle_;
                other.handle = nullptr;
            }

            return *this;
        }

        ~task() {
            if(handle_) {
                handle_.destroy();
            }
        }

        [[nodiscard]] inline bool is_ready() const noexcept {
            return !handle_ || handle_.done();
        }

        auto operator co_await() const & noexcept {
            struct awaiter : awaiter_base {
                decltype(auto) await_resume() {
                    return this->handle.promise().result();
                }
            };

            return awaiter{handle_};
        }

        auto operator co_await() const && noexcept {
            struct awaiter : awaiter_base {
                decltype(auto) await_resume() {
                    return std::move(this->handle.promise().result());
                }
            };

            return awaiter{handle_};
        }

        [[nodiscard]] auto when_ready() const noexcept {
            struct awaiter : awaiter_base {
                decltype(auto) await_resume() const noexcept {}
            };

            return awaiter{handle_};
        }

        std::coroutine_handle<promise_type> get_handle() const noexcept {
            return handle_;
        }

        void detach() noexcept {
            if constexpr (std::is_void_v<value_type>) {
                handle_.promise().is_detached = true;
            }

            handle_ = nullptr;
        }

        friend void swap(task& a, task& b) {
            std::swap(a.handle_, b.handle_);
        }
    private:
        std::coroutine_handle<promise_type> handle_;
    };

namespace detail {
    template<typename T>
    inline task<T> task_promise<T>::get_return_object() noexcept {
        return task<T>{
            std::coroutine_handle<task_promise>::from_promise(*this)
        };
    }

    inline task<void> task_promise<void>::get_return_object() noexcept {
        return task<void>{
            std::coroutine_handle<task_promise>::from_promise(*this)
        };
    }

    template<typename T>
    inline task<T &> task_promise<T &>::get_return_object() noexcept {
        return task<T &>{
            std::coroutine_handle<task_promise>::from_promise(*this)
        };
    }
} // detail

    template<typename Awaiter>
    auto make_task(Awaiter awaiter) -> task<detail::remove_rvalue_reference_t<detail::get_awaiter_result_t<Awaiter>>> {
        co_return co_await static_cast<Awaiter &&>(awaiter);
    }

} // coro
