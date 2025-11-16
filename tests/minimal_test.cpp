#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>

#include <liburing/io_service.hpp>
#include <liburing/task.hpp>
#include <liburing/utils.hpp>

using namespace std::chrono_literals;

// 全局IOService指针
coro::IOService* g_service = nullptr;

// 记录程序开始时间
std::chrono::steady_clock::time_point program_start_time;

// 获取时间戳
std::string get_timestamp() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - program_start_time);
    double ms = duration.count() / 1000.0;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << ms << "ms";
    return ss.str();
}

// 日志打印
void log_with_timestamp(const std::string& message) {
    std::cout << "[" << get_timestamp() << "] " << message << std::endl;
}

// 简单的延迟任务
coro::Task<int> simple_delay_task(std::chrono::milliseconds delay, int value) {
    log_with_timestamp("Starting task with delay: " + std::to_string(delay.count()) + "ms");
    
    __kernel_timespec ts = coro::dur2ts(std::chrono::duration_cast<std::chrono::nanoseconds>(delay));
    co_await g_service->timeout(&ts);
    
    log_with_timestamp("Task completed with value: " + std::to_string(value));
    co_return value;
}

// 测试单个任务的主协程
coro::Task<> main_coro() {
    log_with_timestamp("=== Starting main coroutine ===");
    
    // 只运行单个任务，不使用when_all/when_any
    auto task = simple_delay_task(1000ms, 42);
    int result = co_await task;
    
    log_with_timestamp("Received result: " + std::to_string(result));
    log_with_timestamp("=== Main coroutine completed ===");
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
        
        log_with_timestamp("IOService initialized");
        
        // 运行主协程
        auto main_task = main_coro();
        service.run(main_task);
        
        log_with_timestamp("Program completed successfully!");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}