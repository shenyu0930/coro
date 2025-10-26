#include <liburing/utils.hpp>
#include <liburing/io_service.hpp>
#include <string_view>
#include <iostream>

auto ping(coro::IOService& service, int read_fd, int write_fd) -> coro::Task<> {
    std::string_view msg = "ping!";
    std::string_view expected = "pong!";
    std::array<char, 64> buffer;

    for (int i = 0; i < 20; i++) {
        int count =
            co_await service.read(read_fd, buffer.data(), buffer.size(), 0)
            | coro::PanicOnErr("ping: Unable to read from read_fd", false);

        auto recieved = std::string_view(buffer.data(), count);

        std::cout << std::format("ping: Recieved {}\n", recieved) << std::endl;
        if (recieved != expected)
            coro::Panic("Unexpected message", 0);

        co_await service.write(write_fd, msg.data(), msg.size(), 0)
            | coro::PanicOnErr("ping: Unable to write to write_fd", false);
    }

    co_await service.close(write_fd)
        | coro::PanicOnErr("ping: Unable to close write_fd", false);

    // Check for EOF before exiting
    if(0 != co_await service.read(read_fd, buffer.data(), buffer.size(), 0)) {
        throw std::runtime_error("pong: Pipe not at EOF like expected");
    }

    co_await service.close(read_fd)
        | coro::PanicOnErr("ping: Unable to close read_fd", false);
}

auto pong(coro::IOService& service, int read_fd, int write_fd) -> coro::Task<> {
    std::string_view msg = "pong!";
    std::string_view expected = "ping!";
    std::array<char, 64> buffer;

    for (int i = 0; i < 20; i++) {
        co_await service.write(write_fd, msg.data(), msg.size(), 0)
            | coro::PanicOnErr("pong: Unable to write to write_fd", false);

        int count =
            co_await service.read(read_fd, buffer.data(), buffer.size(), 0)
            | coro::PanicOnErr("pong: Unable to read from read_fd", false);

        auto recieved = std::string_view(buffer.data(), count);

        std::cout << std::format("pong: Recieved {}\n", recieved) << std::endl;
        if (recieved != expected)
            coro::Panic("Unexpected message", 0);
    }

    co_await service.close(write_fd)
        | coro::PanicOnErr("pong: Unable to close write_fd", false);

    // Check for EOF before exiting
    if(0 != co_await service.read(read_fd, buffer.data(), buffer.size(), 0)) {
        throw std::runtime_error("pong: Pipe not at EOF like expected");
    }

    co_await service.close(read_fd)
        | coro::PanicOnErr("pong: Unable to close read_fd", false);
}

int main() {
    using coro::IOService;
    using coro::Task;

    IOService io;

    std::array<int, 2> p1;
    std::array<int, 2> p2;
    pipe(p1.data()) | coro::PanicOnErr("Unable to open pipe", true);
    pipe(p2.data()) | coro::PanicOnErr("Unable to open pipe", true);

    // ping reads from p1 and writes to p2
    auto t1 = ping(io, p1[0], p2[1]);
    // pong writes to p1 and reads from p2
    auto t2 = pong(io, p2[0], p1[1]);

    io.run(t1);
    io.run(t2);
}