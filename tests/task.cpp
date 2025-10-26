#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <chrono>
#include <utility>
#include <mutex>
#include <list>
#include <iostream>

#include "io_utils.h"

using namespace std::chrono_literals;

template <typename ResultType>
struct Task;

template <typename Result>
struct TaskAwaiter;

template <typename T>
struct Result {
    explicit Result() = default;
    explicit Result(T&& value) 
        : value_(value) {}
    explicit Result(std::exception_ptr&& exception_ptr) : exception_ptr_(exception_ptr) {}

    T get_or_throw() {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }

        return value_;
    }

private:
    T value_{};
    std::exception_ptr exception_ptr_;
};

template <typename ResultType>
struct TaskPromise {
    std::suspend_never initial_suspend() {
        return {};
    }

    std::suspend_always final_suspend() noexcept {
        return {};
    }

    Task<ResultType> get_return_object() {
        return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    void unhandled_exception() {
        std::lock_guard lock(completion_lock_);
        result_ = Result<ResultType>(std::current_exception());
        completion_.notify_all();
        notify_callbacks();
    }

    void return_value(ResultType value) {
        debug("get return value", value);
        std::lock_guard lock(completion_lock_);
        result_ = Result<ResultType>(std::move(value));
        completion_.notify_all();
        notify_callbacks();
    }

    ResultType get_result() {
        std::unique_lock lock(completion_lock_);
        if (!result_.has_value()) {
            completion_.wait(lock);
        }

        return result_->get_or_throw();
    }

    void on_completed(std::function<void(Result<ResultType>)>&& func) {
        std::unique_lock lock(completion_lock_);
        if (result_.has_value()) {
            debug("hello world")
            auto value = result_.value();
            lock.unlock();
            func(value);
        }
        else {
            completion_callbacks_.push_back(func);
        }
    }

    template <typename _ResultType>
    TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
        return TaskAwaiter<_ResultType>(std::move(task));
    }
private:
    std::optional<Result<ResultType>> result_;

    std::mutex completion_lock_;
    std::condition_variable completion_;

    std::list<std::function<void(Result<ResultType>)>> completion_callbacks_;

    void notify_callbacks() {
        auto value = result_.value();
        for (auto& callback : completion_callbacks_) {
            callback(value);
        }

        completion_callbacks_.clear();
    }
};

template <typename Result>
struct TaskAwaiter {
    explicit TaskAwaiter(Task<Result>&& task) noexcept 
        : task_(std::move(task)) {
    }

    TaskAwaiter(TaskAwaiter&& completion) noexcept 
        : task_(std::exchange(completion.task_, {})) {
    }

    TaskAwaiter(TaskAwaiter&) = delete;
    TaskAwaiter& operator=(TaskAwaiter&) = delete;

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        task_.finally([handle] {
            debug("finally");
            handle.resume();
        });
    }

    Result await_resume() noexcept {
        return task_.get_result();
    }
private:
    Task<Result> task_;
};

template <typename ResultType>
struct Task {
    using promise_type = TaskPromise<ResultType>;

    ResultType get_result() {
        return handle_.promise().get_result();
    }

    Task& then(std::function<void(ResultType)>&& func) {
        handle_.promise().on_completed([func](auto result) {
            try {
                func(result.get_or_throw());
            } catch(std::exception& e) {
                // ignore
            }
        });
        return *this;
    }

    Task& catching(std::function<void(std::exception&)>&& func) {
        handle_.promise().on_completed([func](auto result) {
            try {
                result.get_or_throw();
            } catch(std::exception& e) {
                func(e);
            }
        });

        return *this;
    }

    Task& finally(std::function<void()>&& func) {
        handle_.promise().on_completed([func](auto result) {
            func();
        });

        return *this;
    }

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept: handle_(handle) {}

    Task(Task &&task) noexcept: handle_(std::exchange(task.handle_, {})) {}

    Task(Task &) = delete;

    Task &operator=(Task &) = delete;

    ~Task() {
      if (handle_) handle_.destroy();
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

Task<int> simple_task2() {
  debug("task 2 start ...");
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1s);
  debug("task 2 returns after 1s.");
  co_return 2;
}

Task<int> simple_task3() {
  debug("in task 3 start ...");
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);
  debug("task 3 returns after 2s.");
  co_return 3;
}

Task<int> simple_task() {
  debug("task start ...");
  auto result2 = co_await simple_task2();
  debug("returns from task2: ", result2);
  auto result3 = co_await simple_task3();
  debug("returns from task3: ", result3);
  co_return 1 + result2 + result3;
}

int main() {
  auto simpleTask = simple_task();
  simpleTask.then([](int i) {
    debug("simple task end: ", i);
  }).catching([](std::exception &e) {
    debug("error occurred", e.what());
  });
  //try {
  //  auto i = simpleTask.get_result();
  //  debug("simple task end from get: ", i);
  //} catch (std::exception &e) {
  //  debug("error: ", e.what());
  //}
  return 0;
}