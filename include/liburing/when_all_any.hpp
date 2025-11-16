#pragma once

#include <atomic>
#include <tuple>
#include <variant>
#include <coroutine>
#include <type_traits>
#include <cassert>
#include <exception>
#include <utility>

#include "task.hpp"

namespace coro {

// 辅助元函数：获取Task的返回类型
template <typename T>
struct task_result_type;

template <typename T, bool nothrow>
struct task_result_type<Task<T, nothrow>> {
    using type = T;
};

template <typename T>
using task_result_type_t = typename task_result_type<T>::type;

// 辅助元函数：处理void类型
template <typename T>
using non_void_type_t = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

// WhenAllAwaiter类 - 简化版本，修复内存管理和协程恢复问题
template <typename... Tasks>
class WhenAllAwaiter {
public:
    using TasksTuple = std::tuple<Tasks...>;
    using ResultsTuple = std::tuple<non_void_type_t<task_result_type_t<std::decay_t<Tasks>>>...>;
    
    TasksTuple tasks_;
    std::atomic<size_t> completed_count_{0};
    std::atomic<bool> any_exception_{false};
    std::exception_ptr exception_ptr_;
    ResultsTuple results_;
    std::coroutine_handle<> continuation_;
    std::atomic<bool> resumed_{false};

    WhenAllAwaiter(Tasks&&... tasks) 
        : tasks_(std::forward<Tasks>(tasks)...) {}
    
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> continuation) {
        continuation_ = continuation;
        
        // 使用索引序列展开所有任务
        await_all_tasks(std::index_sequence_for<Tasks...>{});
    }

    // 辅助方法：使用索引序列展开所有任务
    template <size_t... Is>
    void await_all_tasks(std::index_sequence<Is...>) {
        (await_task_impl<std::tuple_element_t<Is, TasksTuple>, Is>(std::get<Is>(tasks_)), ...);
    }

    template <typename TaskType, size_t I>
    void await_task_impl(TaskType& task) {
        // 启动一个协程来等待单个任务完成
        [this, &task]() -> Task<> {
            try {
                if constexpr (!std::is_void_v<task_result_type_t<TaskType>>) {
                    // 非void任务，存储结果
                    std::get<I>(results_) = co_await task;
                } else {
                    // void任务，只需等待完成
                    co_await task;
                }
            } catch (...) {
                // 捕获异常并设置异常标志
                if (!any_exception_.exchange(true)) {
                    exception_ptr_ = std::current_exception();
                }
            }

            // 增加完成计数，当所有任务完成时恢复调用者
            // 使用原子操作确保只恢复一次
            if (++completed_count_ == sizeof...(Tasks)) {
                bool expected = false;
                if (!resumed_.exchange(true)) {
                    continuation_.resume();
                }
            }
        }();
    }

    auto await_resume() {
        // 如果有异常，抛出第一个遇到的异常
        if (any_exception_) {
            std::rethrow_exception(exception_ptr_);
        }

        // 特殊情况：单个任务时直接返回结果
        if constexpr (sizeof...(Tasks) == 1) {
            using FirstTaskType = std::tuple_element_t<0, TasksTuple>;
            using FirstResultType = task_result_type_t<FirstTaskType>;
            
            if constexpr (std::is_void_v<FirstResultType>) {
                return;
            } else {
                return std::get<0>(results_);
            }
        } else {
            // 多个任务时返回结果元组
            return results_;
        }
    }
};

// when_all实现 - 等待所有任务完成并收集结果
template <typename... Tasks>
auto when_all(Tasks&&... tasks) {
    return WhenAllAwaiter<std::decay_t<Tasks>...>(std::forward<Tasks>(tasks)...);
}

// WhenAnyAwaiter类 - 简化版本，修复内存管理和协程恢复问题
template <typename... Tasks>
class WhenAnyAwaiter {
public:
    using TasksTuple = std::tuple<Tasks...>;
    using ResultVariant = std::variant<non_void_type_t<task_result_type_t<std::decay_t<Tasks>>>...>;
    
    TasksTuple tasks_;
    std::atomic<bool> completed_{false};
    size_t completed_index_{0};
    std::exception_ptr exception_ptr_;
    ResultVariant result_;
    std::coroutine_handle<> continuation_;

    WhenAnyAwaiter(Tasks&&... tasks) 
        : tasks_(std::forward<Tasks>(tasks)...) {}
    
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> continuation) {
        continuation_ = continuation;
        
        // 使用索引序列展开所有任务
        await_all_tasks(std::index_sequence_for<Tasks...>{});
    }

    // 辅助方法：使用索引序列展开所有任务
    template <size_t... Is>
    void await_all_tasks(std::index_sequence<Is...>) {
        (await_task_impl<std::tuple_element_t<Is, TasksTuple>, Is>(std::get<Is>(tasks_)), ...);
    }

    template <typename TaskType, size_t I>
    void await_task_impl(TaskType& task) {
        [this, &task]() -> Task<> {
            try {
                if constexpr (!std::is_void_v<task_result_type_t<TaskType>>) {
                    // 非void任务，获取结果
                    auto value = co_await task;
                    
                    // 检查是否是第一个完成的任务
                    bool expected = false;
                    if (completed_.compare_exchange_strong(expected, true)) {
                        completed_index_ = I;
                        result_.template emplace<I>(std::move(value));
                        continuation_.resume();
                    }
                } else {
                    // void任务，只需等待完成
                    co_await task;
                    
                    // 检查是否是第一个完成的任务
                    bool expected = false;
                    if (completed_.compare_exchange_strong(expected, true)) {
                        completed_index_ = I;
                        result_.template emplace<I>(std::monostate{});
                        continuation_.resume();
                    }
                }
            } catch (...) {
                // 检查是否是第一个遇到异常的任务
                bool expected = false;
                if (completed_.compare_exchange_strong(expected, true)) {
                    exception_ptr_ = std::current_exception();
                    continuation_.resume();
                }
            }
        }();
    }

    // 返回结果结构体，包含索引和值
    struct WhenAnyResult {
        size_t index; // 完成的任务索引
        ResultVariant value; // 完成任务的结果
    };

    WhenAnyResult await_resume() {
        // 如果有异常，抛出
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        return {completed_index_, result_};
    }
};

// when_any实现 - 等待任意一个任务完成并返回其结果
template <typename... Tasks>
auto when_any(Tasks&&... tasks) {
    return WhenAnyAwaiter<std::decay_t<Tasks>...>(std::forward<Tasks>(tasks)...);
}

} // namespace coro