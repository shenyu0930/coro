#include <initializer_list>
#include <iostream>
#include <coroutine>
#include <type_traits>
#include <utility>
#include <list>
#include <functional>
#include <string>

template <typename T>
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

        std::suspend_always yield_value(T value) {
            this->value = value;
            is_ready = true;
            return {};
        }

        T value{};
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
        if(handle) {
            handle.destroy();
        }
    }

    Generator static from_array(T array[], int n) {
        for (int i = 0; i < n; i++) {
            std::cout << "co_yield " << array[i] << std::endl;
            co_yield array[i];
        }
    }
    
    Generator static from_list(std::list<T> list) {
        for (auto t : list) {
            co_yield t;
        }
    }

    Generator static from(std::initializer_list<T> args) {
        for (auto arg : args) {
            co_yield arg;
        }
    }

    template <typename ...Args>
    Generator static from(Args&& ...args) {
        (co_yield args, ...);
    }

    //template <typename U>
    //Generator<U> map(std::function<U(T)> f) {
    //    auto up_stream = std::move(*this);
    //    while (up_stream.has_next()) {
    //        co_yield f(up_stream.next());
    //    }
    //}

    template <typename F>
    Generator<std::invoke_result_t<F, T>> map(F f) {
        auto up_stream = std::move(*this);
        while (up_stream.has_next()) {
            co_yield f(up_stream.next());
        }
    }

    template <typename F>
    std::invoke_result_t<F, T> flat_map(F f) {
        auto up_stream = std::move(*this);
        while (up_stream.has_next()) {
            auto generator = f(up_stream.next());
            while (generator.has_next()) {
                co_yield generator.next();
            }
        }
    }

    template <typename F>
    void for_each(F f) {
        while (has_next()) {
            f(next());
        }
    }

    template <typename R, typename F>
    R fold(R initial, F f) {
        R acc = initial;
        while (has_next()) {
            acc = f(acc, next());
        }
        return acc;
    }

    T sum() {
        T sum = 0;
        while (has_next()) {
            sum += next();
        }

        return sum;
    }

    template <typename F>
    Generator filter(F f) {
        int i = 0;
        auto up_stream = std::move(*this);
        while (up_stream.has_next()) {
            std::cout << "helo " << i++ << std::endl;
            T value = up_stream.next();
            if (f(value)) {
                co_yield value;
            }
        }
    }

    Generator take(int n) {
        auto up_stream = std::move(*this);
        int i = 0;
        while (i++ < n && up_stream.has_next()) {
            co_yield up_stream.next();
        }
    }

    template <typename F>
    Generator take_while(F f) {
        auto up_stream = std::move(*this);
        while (up_stream.has_next()) {
            T value = up_stream.next();
            if (f(value)) {
                co_yield value;
            } else {
                break;
            }
        }
    }

    T next() {
        if (has_next()) {
            handle.promise().is_ready = false;
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

    void await_suspend(std::coroutine_handle<Generator<int>::promise_type> handle) noexcept {
        handle.promise().value = value;
    }

    void await_resume() {}
};

//auto operator co_await(int value) {
//    return IntAwaiter{.value = value};
//}

Generator<int> sequence() {
    int i = 0;
    while (true)
    {
        co_yield i++;
    }
}

Generator<int> fibonacci() {
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

    // array
    //int array[] = {1, 2, 3, 4, 5};
    //auto seq = Generator<int>::from_array(array, 5);

    // std::list
    // auto seq = Generator<int>::from_list(std::list{1, 2, 3, 4, 5});

    // initializer_list
    // auto seq = Generator<int>::from({1,2 ,3 ,4 ,5});

    // fold expression 
    // auto seq = Generator<int>::from(1, 2, 3, 4, 5);
    
    // map
    //auto seq = fibonacci().map<std::string>([](auto value) {
    //    return std::to_string(value);
    //});

    //auto seq = fibonacci().map([](auto value) {
    //    return std::to_string(value);
    //});

    //for (int i = 0; i < 10; i++) {
    //    std::cout << seq.next() << std::endl;
    //}

    // flat_map
    //Generator<int>::from(1, 2, 3, 4)
    //.flat_map([](auto i) -> Generator<int> {
    //    for (int j = 0; j < i; ++j) {
    //        co_yield j; 
    //    }
    //})
    //.for_each([](auto i) {
    //    if (i == 0) {
    //        std::cout << std::endl;
    //    }
    //    std::cout << "* ";
    //});

    // fold
    // auto result = Generator<int>::from(1, 2, 3, 4, 5).fold(1, [](auto acc, auto i) {
    //     return acc * i;
    // });
    // std::cout << "acc[" << result << "]" << std::endl;

    // sum
    // auto result = Generator<int>::from(1, 2, 3, 4, 5).sum();
    // std::cout << "sum[" << result << "]" << std::endl;

    // filter
    // take
    Generator<int>::from(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    .filter([](auto i) {
      std::cout << "filter: " << i << std::endl;
      return i % 2 == 0;
    })
    .map([](auto i) {
      std::cout << "map: " << i << std::endl;
      return i * 3;
    })
    .flat_map([](auto i) -> Generator<int> {
      std::cout << "flat_map: " << i << std::endl;
      for (int j = 0; j < i; ++j) {
        co_yield j;
      }
    }).take(30)
    .for_each([](auto i) {
      std::cout << "for_each: " << i << std::endl;
    });

    return 0;
}