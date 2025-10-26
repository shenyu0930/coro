#include <iostream>
#include <coroutine>
#include <utility>
#include <chrono>

using namespace std::chrono_literals;

struct SuspendAlways 
{
    bool await_ready() const noexcept { return false; }

    bool await_suspend(std::coroutine_handle<>) const noexcept {
        std::cout << "await_suspend" << std::endl;
        return true;
    }

    void await_resume() const noexcept {
        std::cout << "await_resume" << std::endl;
    }
};

struct Generator {
    struct ExaustedException : std::exception {
        
    };

    struct promise_type {
        std::suspend_never initial_suspend() {
            return {};
        }
        
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        
        void return_void() {
        }
        
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        void unhandled_exception() {}

        //SuspendAlways await_transform(int value) {
        //    std::cout << "create suspend always" << std::endl;
        //    this->value = value;
        //    return {};
        //}    

        SuspendAlways yield_value(int value) {
            std::cout << "yield suspend always" << std::endl;
            this->value = value;
            is_ready = true;
            return {};
        }
        
        int value{};
        bool is_ready{false};
    };

    explicit Generator(std::coroutine_handle<promise_type> handle) noexcept 
        : handle(handle) {
    }

    Generator(Generator&& generator) noexcept 
        : handle(std::exchange(generator.handle, {})) {
    }

    Generator(Generator&) = delete;
    Generator& operator=(Generator&) = delete;

    ~Generator() {
        handle.destroy();
    }
    
    int next() {
        if (has_next()) {
            std::cout << "before next" << std::endl;
            handle.promise().is_ready = false;
            std::cout << "after next" << std::endl;
            return handle.promise().value;
        }

        throw ExaustedException();
    }

    bool has_next() {
        if (!handle || handle.done()) {
            return false;
        }

        if (!handle.promise().is_ready) {
            handle.resume();
        }

        if (handle.done()) {
            return false;
        }

        return true;
    }
    
    std::coroutine_handle<promise_type> handle;
};

struct IntAwaiter {
    int value;

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<Generator::promise_type> handle) noexcept {
        handle.promise().value = value;
    }

    void await_resume() {}
};

//auto operator co_await(int value) {
//    return IntAwaiter{.value = value};
//}

Generator sequence() {
    std::cout << "sequence begin" << std::endl;
    int i = 0;
    while (true)
    {
        co_yield i++;
    }
}

Generator fibonacci() {
    co_yield 0;
    co_yield 1;

    int a = 0;
    int b = 1;
    while (true) {
        co_yield a + b;
        b = a + b;
        a = b - a;
    }
}

int main() {
    std::cout << "all begin" << std::endl;
    auto seq = fibonacci();

    for (int i = 0; i < 10; i++) {
        std::cout << seq.next() << std::endl;
    }

    return 0;
}