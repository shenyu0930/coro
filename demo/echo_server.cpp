#include "liburing/utils.hpp"
#include <unistd.h>
#include <iostream>
#include <format>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <vector>
#include <poll.h>
#include <numeric>

#include <liburing/io_service.hpp>

enum {
    BUF_SIZE = 512,
    MAX_CONN_SIZE = 512
};

int runningCoroutines = 0;

coro::Task<> accept_connection(coro::IOService& service, int serverfd) {
    while (int clientfd = co_await service.accept(serverfd, nullptr, nullptr)) {
        [](coro::IOService& service, int clientfd) -> coro::Task<> {
            std::cout << std::format("sock {} accepted; number of running coroutines: {}\n", 
                clientfd, ++runningCoroutines);

            std::vector<char> buf(BUF_SIZE);
            while (true) {
                co_await service.poll(clientfd, POLLIN);
                int r = co_await service.recv(clientfd, buf.data(), buf.size(), MSG_NOSIGNAL);
                if (r <= 0) break;
                co_await service.send(clientfd, buf.data(), r, MSG_NOSIGNAL);
            }

            service.shutdown(clientfd, SHUT_RDWR, IOSQE_IO_LINK);
            co_await service.close(clientfd);
            std::cout << std::format("sockfd {} is closed; number of running coroutines: {}\n",
                clientfd, --runningCoroutines);
        }(service, clientfd);
    }
}

int main(int argc, char* argv[]) {
    uint16_t server_port = 0;
    if (argc == 2) {
        server_port = (uint16_t)std::strtoul(argv[1], nullptr, 10);
    }
    if (server_port == 0) {
        std::cerr << "Usage: " << argv[0] << " <server_port>\n";
        return 1;
    }

    coro::IOService service(MAX_CONN_SIZE);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0) | coro::PanicOnErr("socket creation", true);
    coro::OnScopeExit closesock([=](){shutdown(sockfd, SHUT_RDWR);});

    if (sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(server_port),
        .sin_addr = {INADDR_ANY},
        .sin_zero = {0}
    }; bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) coro::Panic("bind", errno);

    if (listen(sockfd, MAX_CONN_SIZE * 2)) coro::Panic("listen", errno);
    std::cout << std::format("Listening: {}\n", server_port);

    service.run(accept_connection(service, sockfd));
}