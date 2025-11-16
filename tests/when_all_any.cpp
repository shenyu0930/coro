// 简化版本的when_all_any测试
#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>

#include <liburing/io_service.hpp>
#include <liburing/task.hpp>
#include <liburing/when_all_any.hpp>
#include <liburing/utils.hpp>

using namespace std::chrono_literals;

// 全局IOService指针，用于延迟任务中访问
coro::IOService* g_service = nullptr;

// 记录程序开始时间的时间点
std::chrono::steady_clock::time_point program_start_time;

// 获取当前时间戳（相对于程序启动），用于日志
std::string get_timestamp() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - program_start_time);
    double ms = duration.count() / 1000.0;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << ms << "ms";
    return ss.str();
}

// 带时间戳的日志打印
void log_with_timestamp(const std::string& message) {
    std::cout << "[" << get_timestamp() << "] " << message << std::endl;
}

// 简化的异步延迟任务 - 只使用基本类型
coro::Task<int> delayed_task_int(std::chrono::milliseconds delay, int value) {
    log_with_timestamp("Starting delayed task, delay: " + std::to_string(delay.count()) + "ms, value: " + std::to_string(value));
    
    auto start_time = std::chrono::steady_clock::now();
    __kernel_timespec ts = coro::dur2ts(std::chrono::duration_cast<std::chrono::nanoseconds>(delay));
    co_await g_service->timeout(&ts);
    auto end_time = std::chrono::steady_clock::now();
    
    auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    log_with_timestamp("Delayed task completed, actual delay: " + std::to_string(actual_delay.count()) + "ms, value: " + std::to_string(value));
    
    co_return value;
}

// 简化的异步延迟任务 - 只使用字符串类型
coro::Task<std::string> delayed_task_string(std::chrono::milliseconds delay, std::string value) {
    log_with_timestamp("Starting delayed task, delay: " + std::to_string(delay.count()) + "ms, value: " + value);
    
    auto start_time = std::chrono::steady_clock::now();
    __kernel_timespec ts = coro::dur2ts(std::chrono::duration_cast<std::chrono::nanoseconds>(delay));
    co_await g_service->timeout(&ts);
    auto end_time = std::chrono::steady_clock::now();
    
    auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    log_with_timestamp("Delayed task completed, actual delay: " + std::to_string(actual_delay.count()) + "ms, value: " + value);
    
    co_return value;
}

// 异步延迟任务，不返回值
coro::Task<> void_task(int delay_ms, const std::string& message) {
    auto delay = std::chrono::milliseconds(delay_ms);
    log_with_timestamp("Starting void task, delay: " + std::to_string(delay_ms) + "ms, message: " + message);
    
    auto start_time = std::chrono::steady_clock::now();
    __kernel_timespec ts = coro::dur2ts(std::chrono::duration_cast<std::chrono::nanoseconds>(delay));
    co_await g_service->timeout(&ts);
    auto end_time = std::chrono::steady_clock::now();
    
    auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    log_with_timestamp("Void task completed, actual delay: " + std::to_string(actual_delay.count()) + "ms, message: " + message);
}

// 简化的when_all测试 - 只使用相同类型
coro::Task<> demo_when_all_simple() {
    log_with_timestamp("=== Testing when_all (simple) ===");
    auto start_time = std::chrono::steady_clock::now();
    
    // 创建多个延迟任务 - 只使用整数类型
    auto task1 = delayed_task_int(3000ms, 42);
    auto task2 = delayed_task_int(2000ms, 100);
    auto task3 = delayed_task_int(2500ms, 200);
    
    log_with_timestamp("when_all: All tasks created, waiting for completion");
    
    // 使用when_all等待所有任务完成
    auto [result1, result2, result3] = co_await coro::when_all(
        std::move(task1),
        std::move(task2),
        std::move(task3)
    );
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    log_with_timestamp("when_all completed! Total time: " + std::to_string(duration.count()) + "ms");
    log_with_timestamp("Result1: " + std::to_string(result1));
    log_with_timestamp("Result2: " + std::to_string(result2));
    log_with_timestamp("Result3: " + std::to_string(result3));
}

// 简化的when_any测试 - 只使用相同类型
coro::Task<> demo_when_any_simple() {
    log_with_timestamp("=== Testing when_any (simple) ===");
    auto start_time = std::chrono::steady_clock::now();
    
    // 创建多个延迟不同时间的任务 - 只使用整数类型
    auto task1 = delayed_task_int(3000ms, 1);
    auto task2 = delayed_task_int(1000ms, 2);  // 这个应该最先完成
    auto task3 = delayed_task_int(2000ms, 3);
    
    log_with_timestamp("when_any: All tasks created, waiting for first completion");
    
    // 使用when_any等待任意一个任务完成
    auto result = co_await coro::when_any(
        std::move(task1),
        std::move(task2),
        std::move(task3)
    );
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    log_with_timestamp("when_any completed! Time until first completion: " + std::to_string(duration.count()) + "ms");
    log_with_timestamp("First completed task index: " + std::to_string(result.index));
    
    // 使用std::visit访问variant中的结果
    std::visit([](const auto& value) {
        log_with_timestamp("First completed task result: " + std::to_string(value));
    }, result.value);
}

// 主协程 - 只运行简化版本的测试
coro::Task<> main_coro() {
    log_with_timestamp("=== Starting main coroutine ===");
    
    // 只运行简化版本的测试
    co_await demo_when_all_simple();
    co_await demo_when_any_simple();
    
    log_with_timestamp("=== All tests completed! ===");
}

// 全局IOService指针定义
int main() {
    try {
        // 初始化程序开始时间
        program_start_time = std::chrono::steady_clock::now();
        log_with_timestamp("Program started");
        
        // 创建IOService实例
        coro::IOService service;
        g_service = &service;
        
        log_with_timestamp("IOService initialized, running main task");
        
        // 运行主协程
        auto main_task = main_coro();
        
        log_with_timestamp("Event loop started");
        
        // 使用service.run()方法运行任务直到完成
        service.run(main_task);
        
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - program_start_time);
        
        log_with_timestamp("Program completed successfully! Total execution time: " + 
                         std::to_string(total_time.count()) + "ms");
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}