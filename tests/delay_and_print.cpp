#include <chrono>
#include <format>
#include <iostream>

#include "liburing/io_service.hpp"

int main() {
    using coro::IOService;
    using coro::Task;
    using coro::PanicOnErr;
    using coro::dur2ts;

    IOService service;

    service.run([] (IOService& service) -> Task<> {
        auto delayAndPrint = [&] (int second, uint8_t iflags = 0) -> Task<> {
            auto ts = dur2ts(std::chrono::seconds(second));
            co_await service.timeout(&ts, iflags) | PanicOnErr("timeout", false);
            std::cout << std::format("{:%T}: delayed {}s\n", std::chrono::system_clock::now().time_since_epoch(), second) << std::endl;
        };

        std::cout << std::format("in sequence start\n") << std::endl;
        co_await delayAndPrint(1);
        co_await delayAndPrint(2);
        co_await delayAndPrint(3);
        std::cout << std::format("in sequence end, should wait 6s\n\n") << std::endl;

        std::cout << std::format("io link start\n") << std::endl;
        delayAndPrint(1, IOSQE_IO_HARDLINK);
        delayAndPrint(2, IOSQE_IO_HARDLINK);
        co_await delayAndPrint(3);
        std::cout << std::format("io link end, should wait 6s\n\n") << std::endl;
    }(service));
}