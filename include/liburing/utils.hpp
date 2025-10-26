#pragma once

#include <cerrno>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <string_view>
#include <time.h>
#include <execinfo.h>
#include <stdio.h>
#include <chrono>

#include "sqe_awaitable.hpp"
#include "task.hpp"


namespace coro {

template <typename Fn>
struct OnScopeExit {
    OnScopeExit(Fn &&fn) 
        : fn_(std::move(fn)) {}
    ~OnScopeExit() { 
        this->fn_(); 
    }

private:
    Fn fn_;
};

[[noreturn]]
void Panic(std::string_view sv, int err) {
#ifndef NDEBUG
    // https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
    void *array[32];
    size_t size{};

    // get void*'s for all entries on the stack
    size = backtrace(array, 32);

    // print all the frames on the stack
    fprintf(stderr, "error: errno[%d]\n", err);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

    throw std::system_error(err, std::generic_category(), sv.data());
}

struct PanicOnErr {
    PanicOnErr(std::string_view command, bool use_errno)
        : command(command)
        , use_errno(use_errno) {}
    std::string_view command;
    bool use_errno;
};

inline int operator |(int ret, PanicOnErr&& poe) {
    if (ret < 0) {
        if (poe.use_errno) {
            Panic(poe.command, errno);
        } else {
            if (ret != -ETIME) Panic(poe.command, -ret);
        }
    }

    return ret;
}

template <bool nothrow>
inline Task<int> operator |(Task<int, nothrow> tret, PanicOnErr&& poe) {
    co_return (co_await tret) | std::move(poe);
}

inline Task<int> operator |(SqeAwaitable tret, PanicOnErr&& poe) {
    co_return (co_await tret) | std::move(poe);
}

[[nodiscard]]
constexpr inline __kernel_timespec dur2ts(std::chrono::nanoseconds dur) noexcept {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    dur -= secs;
    return { secs.count(), dur.count() };
}

}