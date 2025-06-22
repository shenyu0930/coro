#include <iostream>

#include <liburing.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include <map>
#include <format>
#include <string>

constexpr int max_entry_cnt = 128;
constexpr int buffer_size = 4096;
constexpr int max_client_cnt = 10;
unsigned short port{};

io_uring* ring = nullptr;

enum class ConnectionType {
    ACCEPT,
    READ,
    WRITE
};

struct ConnectionObject {
    int fd_{};
    ConnectionType connection_type_{};
    char buffer_[buffer_size]{};
};

class Connection {
public:
    Connection(int socket) 
        : fd_(socket) {

    }

    ~Connection() {
        if(fd_ != -1) {
            ::close(fd_);
        }
    }

    int socket() const {
        return fd_;
    }

    void close() {
        ::close(fd_);
        fd_ = -1;
    }

    void accept(sockaddr* addr, socklen_t* addr_len) {
        cur_accept_connection_object_ = new ConnectionObject();
        cur_accept_connection_object_->fd_ = fd_;
        cur_accept_connection_object_->connection_type_ = ConnectionType::ACCEPT;
        auto sqe = io_uring_get_sqe(ring);
        io_uring_prep_accept(sqe, cur_accept_connection_object_->fd_, addr, addr_len, 0);
        io_uring_sqe_set_data(sqe, cur_accept_connection_object_);
    }

    bool read(int bytes_read) {
        if(cur_read_connection_object_ == nullptr) {
            cur_read_connection_object_ = new ConnectionObject();
            cur_read_connection_object_->fd_ = fd_;
        }
        else {
            if(bytes_read < 0) {
                std::cerr << "read failed" << std::endl;
            } else {
                std::cout << std::format("read: {}", cur_read_connection_object_->buffer_);
            }
        }

        cur_read_connection_object_->connection_type_ = ConnectionType::READ;
        auto sqe = io_uring_get_sqe(ring);
        io_uring_prep_recv(sqe, cur_read_connection_object_->fd_, cur_read_connection_object_->buffer_, buffer_size, 0);
        io_uring_sqe_set_data(sqe, cur_read_connection_object_);

        if(cur_write_connection_object_ == nullptr) {
            cur_write_connection_object_ = new ConnectionObject();
            cur_write_connection_object_->fd_ = fd_;
        }

        memcpy(cur_write_connection_object_->buffer_, cur_read_connection_object_->buffer_, buffer_size);
        memset(cur_read_connection_object_->buffer_, 0, buffer_size);

        return true;
    }

    bool write() {
        if(cur_write_connection_object_ == nullptr) {
            cur_write_connection_object_ = new ConnectionObject();
            cur_write_connection_object_->fd_ = fd_;
        }

        cur_write_connection_object_->connection_type_ = ConnectionType::WRITE; 
        auto sqe = io_uring_get_sqe(ring);
        io_uring_prep_send(sqe, cur_write_connection_object_->fd_, cur_write_connection_object_->buffer_, buffer_size, 0);
        io_uring_sqe_set_data(sqe, &cur_write_connection_object_->fd_); 

        return true;
    }

private:
    int fd_{};
    /*
    char buffer_[buffer_size];
    ssize_t bytes_read_{};
    std::jthread thread_;
    */
    ConnectionObject* cur_read_connection_object_{};
    ConnectionObject* cur_write_connection_object_{};
    ConnectionObject* cur_accept_connection_object_{};

};

class Server {
public:
    Server() {
        if((fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "socket failed" << std::endl;
            return;
        }

        if(setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt_, sizeof(opt_)) < 0) {
            std::cerr << "setsockopt failed" << std::endl;
            close(fd_);
            return;
        }

        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = INADDR_ANY;
        addr_.sin_port = htons(port);
        
        if(bind(fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            std::cerr << "bind failed" << std::endl;
            close(fd_);
            return;
        }

        if(!listen()) {
            std::cerr << "listen failed." << std::endl;
            return;
        }

        ring = new io_uring{};
        if(io_uring_queue_init(max_entry_cnt, ring, 0) < 0) {
            std::cerr << "io uring queue init failed" << std::endl;
            close(fd_);
            return;
        }

        accept();

        init_ = true;
    }

    ~Server() {
        close(fd_);

        if(ring != nullptr) {
            delete ring;
        }

        for(auto& conn : connections_) {
            if(conn.second) {
                conn.second->close();
                delete conn.second;
            }
        }
    }

    bool listen() {
        if(::listen(fd_, max_client_cnt) < 0) {
            close(fd_);
            return false;
        }

        std::cout << std::format("start listening[{}]", port) << std::endl;

        return true;
    }

    void accept() {
        connections_[fd_] = new Connection(fd_);
        connections_[fd_]->accept(reinterpret_cast<sockaddr*>(&addr_), &addr_len_);
        if(io_uring_submit(ring) < 0) {
            std::cerr << "io uring submit failed" << std::endl;
        }
    }

    void run() {
        if(!init_) {
            return;
        }

        io_uring_cqe* cqes[max_entry_cnt];
        while(true) {
            if(io_uring_submit_and_wait(ring, 1) < 0) {
                std::cerr << "io uring submit and wait failed" << std::endl;
                break;
            }

            auto num = io_uring_peek_batch_cqe(ring, cqes, max_entry_cnt);
            for(int i = 0; i < num; ++i) {
                auto obj = reinterpret_cast<ConnectionObject*>(cqes[i]->user_data);

                switch(obj->connection_type_) {
                case ConnectionType::ACCEPT: {
                    std::cout << "connected" << std::endl;

                    auto new_sock = cqes[i]->res;
                    int flags = fcntl(new_sock, F_GETFL, 0);
                    fcntl(new_sock, F_SETFL, flags & ~O_NONBLOCK);
                    connections_[new_sock] = new Connection(new_sock);
                    connections_[new_sock]->read(-1);
                    connections_[obj->fd_]->accept(reinterpret_cast<sockaddr*>(&addr_), &addr_len_);
                    break;
                }
                case ConnectionType::READ: {
                    if(!connections_[obj->fd_]->read(cqes[i]->res)) {
                        connections_[obj->fd_]->close();
                        connections_.erase(obj->fd_);
                    }
                    connections_[obj->fd_]->write();
                    break;
                }
                case ConnectionType::WRITE: {
                    // connections_[obj->fd_]->write();
                    break;
                }
                }
                io_uring_cqe_seen(ring, cqes[i]);
            }
        }

        io_uring_queue_exit(ring);
    }
private:
    int fd_{};
    int opt_{};
    sockaddr_in addr_;
    socklen_t addr_len_{sizeof(sockaddr)};
    bool init_{false};
    std::map<int, Connection*> connections_;
};

int main(int argc, char* argv[]) {
    port = std::atoi(argv[1]);
    Server server{};

    server.run();

    return 0;
}
