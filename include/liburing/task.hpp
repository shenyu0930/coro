#pragma once

#include <exception>
#include <type_traits>
#include <variant>
#include <coroutine>
#include <cassert>
#include <utility>

namespace coro
{
    template <typename T, bool nothrow>
    struct Task;

    template <typename T, bool nothrow>
    struct BaseTaskPromise {
        std::suspend_never initial_suspend() {
            return {};
        }

        auto final_suspend() noexcept {
            struct Awaiter : std::suspend_always {
                BaseTaskPromise* me_;

                Awaiter(BaseTaskPromise* me) : me_(me) {}
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) const noexcept {
                    if (me_->result_.index() == 3) [[unlikely]] {
                        if (me_->waiter_) {
                            me_->waiter_.destroy();
                        }
                        std::coroutine_handle<BaseTaskPromise>::from_promise(*me_).destroy();
                    } else if (me_->waiter_) {
                        return me_->waiter_;
                    }
                    return std::noop_coroutine();
                }
            };

            return Awaiter(this);
        }

        Task<T, nothrow> get_return_object();

        void unhandled_exception() {
            if constexpr (!nothrow) {
                if (result_.index() == 3) [[unlikely]] {
                    return;
                } else {
                    result_.template emplace<2>(std::current_exception());
                }
            } else {
                __builtin_unreachable();
            }
        }

    protected:
        friend class Task<T, nothrow>;
        BaseTaskPromise() = default;
        std::coroutine_handle<> waiter_;
        std::variant<
            std::monostate,
            std::conditional_t<std::is_void_v<T>, std::monostate, T>,
            std::conditional_t<!nothrow, std::exception_ptr, std::monostate>,
            std::monostate  // detached promise
            > result_;
    };

    template <typename T, bool nothrow>
    struct Promise final : public BaseTaskPromise<T, nothrow> {
        using BaseTaskPromise<T, nothrow>::result_; 

        template <typename U>
        void return_value(U&& u) {
            if (result_.index() == 3) [[unlikely]] {
                return;
            }
            result_.template emplace<1>(static_cast<U&&>(u));
        }

        void return_value(int u) {
            if (result_.index() == 3) [[unlikely]] {
                return;
            }
            result_.template emplace<1>(u);
        }
    };
    
    // void specialization
    template <bool nothrow>
    struct Promise<void, nothrow> final : public BaseTaskPromise<void, nothrow> {
        using BaseTaskPromise<void, nothrow>::result_; 

        void return_void() {
            if (result_.index() == 3) [[unlikely]] {
                return;
            }
            result_.template emplace<1>(std::monostate{});
        }
    };

    template <typename T = void, bool nothrow = false>
    struct Task {
        using promise_type = Promise<T, nothrow>;
        using handle_t = std::coroutine_handle<promise_type>;

        Task(Task&& other) : handle_t(std::exchange(other.handle_, {})) {}
        Task& operator=(Task&& other) noexcept {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
            return *this;
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        ~Task() {
            if (!handle_) {
                return;
            } else if (!handle_.done()) {
                handle_.promise().result_.template emplace<3>(std::monostate{});
            } else {
                handle_.destroy();
            }
        }

        bool await_ready() {
            auto& result_ = handle_.promise().result_;
            return result_.index() > 0;
        }
        
        template <typename T_, bool nothrow_>
        void await_suspend(std::coroutine_handle<Promise<T_, nothrow_>> caller) noexcept {
            handle_.promise().waiter_ = caller;
        }

        T await_resume() const {
            return get_result();
        }

        T get_result() const {
            auto& result = handle_.promise().result_;
            if constexpr (!nothrow) {
                if (auto* pep = std::get_if<2>(&result)) {
                    std::rethrow_exception(*pep);
                }
            }

            if constexpr (!std::is_void_v<T>) {
                return *std::get_if<1>(&result);
            }
        }

        bool done() const {
            return handle_.done();
        }

    private:
        friend class BaseTaskPromise<T, nothrow>;
        Task(promise_type* p) : handle_(handle_t::from_promise(*p)) {}
        handle_t handle_;
    };

    template <typename T, bool nothrow>
    Task<T, nothrow> BaseTaskPromise<T, nothrow>::get_return_object() {
        return Task<T, nothrow>(static_cast<Promise<T, nothrow>*>(this));
    }
}