#pragma once

#include <climits>
#include <liburing/io_uring.h>
#include <type_traits>
#include <optional>
#include <cassert>
#include <coroutine>
#include <functional>
#include "liburing.h"

namespace coro {
struct Resolver {
    virtual void resolve(int result) noexcept = 0;
};

struct ResumeResolver final : public Resolver {
    friend struct SqeAwaitable;

    void resolve(int result) noexcept override {
        this->result_ = result;
        handle_.resume();
    }

private:
    std::coroutine_handle<> handle_;
    int result_{};
};

struct DeferredResolver final : public Resolver {
    void resolve(int result) noexcept {
        this->result_ = result;
    }

#ifndef NDEBUG 
    ~DeferredResolver() {
        assert(!!result_ && "DeferredResolver is destructed before it's resolved.");
    }
#endif

    std::optional<int> result_;
};

struct CallbackResolver final : public Resolver {
    CallbackResolver(std::function<void(int)>&& cb) : cb_(std::move(cb)) {}

    void resolve(int result) noexcept {
        cb_(result);
        delete this;
    }
private:
    std::function<void(int)> cb_;
};

struct SqeAwaitable {
    SqeAwaitable(io_uring_sqe* sqe) noexcept : sqe_(sqe) {}
    void set_deferred(DeferredResolver& resolver) {
        io_uring_sqe_set_data(sqe_, &resolver);
    }

    void set_callback(std::function<void(int)> cb) {
        io_uring_sqe_set_data(sqe_, new CallbackResolver(std::move(cb)));
    }
    
    auto operator co_await() {
        struct AwaitSqe {
            ResumeResolver resolver{};
            io_uring_sqe* sqe;

            AwaitSqe(io_uring_sqe* sqe) : sqe(sqe) {}
            
            constexpr bool await_ready() const noexcept { 
                return false;
            }

            void await_suspend(std::coroutine_handle<> handle) noexcept {
                resolver.handle_ = handle;
                io_uring_sqe_set_data(sqe, &resolver);
            }

            constexpr int await_resume() const noexcept {
                return resolver.result_;
            }
        };

        return AwaitSqe(sqe_);
    }
private:
    io_uring_sqe* sqe_;
};
}