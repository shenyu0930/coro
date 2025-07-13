#pragma once

#include "coro/detail/task_info.hpp"
#include "coro/detail/user_data.hpp"
#include <coro/detail/thread_meta.hpp>
#include <coro/detail/worker_meta.hpp>
#include <coroutine>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <utility>
#include <span>


namespace coro::detail {

// for linked sqes
struct lazy_link_io {
    // last awaiter in IOSQE_IO_LINK
    class lazy_awaiter* last_io;
    
    static constexpr bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> current) const noexcept;

    int32_t await_resume() const noexcept;
};

class lazy_awaiter {
public:
    [[nodiscard]] int32_t result() const noexcept {
        return io_info_.result;
    }

    static constexpr bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> current) noexcept {
        io_info_.handle = current;
    }

    lazy_awaiter& set_async() & noexcept {
        auto flags = sqe_->flags;
        io_uring_sqe_set_flags(sqe_, flags | IOSQE_ASYNC);
        return *this;
    }

    lazy_awaiter&& set_async() && noexcept {
        auto flags = sqe_->flags;
        io_uring_sqe_set_flags(sqe_, flags | IOSQE_ASYNC);
        return std::move(*this);
    }

    int32_t await_resume() const noexcept {
        return result();
    }

    std::suspend_never detach() && noexcept {
        // no need user data
        io_uring_sqe_set_data(sqe_, (void*)(uint64_t)(reserved_user_data::nop));
        return {};
    }

    [[nodiscard]] uint64_t user_data() const noexcept {
        return sqe_->user_data;
    }
protected:
    friend struct lazy_link_io;
    friend struct lazy_link_timeout;
    io_uring_sqe* sqe_;
    task_info io_info_;

    friend lazy_link_io operator&&(lazy_awaiter&& lhs, lazy_awaiter&& rhs) noexcept;
    friend lazy_link_io&& operator&&(lazy_link_io&& lhs, lazy_awaiter&& rhs) noexcept;
    friend lazy_link_io&& operator&&(lazy_link_io&& lhs, lazy_link_io&& rhs) noexcept;
    // friend lazy_link_io&& operator&&(lazy_link_io&& lhs, lazy_link_timeout&& rhs) noexcept;
    friend void set_link_awaiter(lazy_awaiter& awaiter) noexcept;

    lazy_awaiter() noexcept : sqe_(this_thread.worker->get_free_sqe()) {
        io_uring_sqe_set_data(sqe_, (void*)(io_info_.as_user_data() | uint64_t(user_data_type::task_info_ptr)));
    }

public:
    lazy_awaiter(const lazy_awaiter&) = delete;
    lazy_awaiter(lazy_awaiter&&) = delete;
    lazy_awaiter& operator=(const lazy_awaiter&) = delete;
    lazy_awaiter& operator=(lazy_awaiter&&) = delete;
};

inline void set_link_sqe(io_uring_sqe* sqe) noexcept {
    auto flags = sqe->flags;
    io_uring_sqe_set_flags(sqe, flags | IOSQE_IO_LINK);
}

inline void set_link_awaiter(lazy_awaiter& awaiter) noexcept {
    set_link_sqe(awaiter.sqe_);
}

inline void set_link_link_io(lazy_link_io& link_io) noexcept {
    set_link_awaiter(*link_io.last_io);
}

inline lazy_link_io operator&&(lazy_awaiter&& lhs, lazy_awaiter&& rhs) noexcept {
    set_link_awaiter(lhs);
    return lazy_link_io{.last_io = &rhs};
}

inline lazy_link_io&& operator&&(lazy_link_io&& lhs, lazy_awaiter&& rhs) noexcept {
    set_link_link_io(lhs);
    lhs.last_io = &rhs;
    return static_cast<lazy_link_io&&>(lhs);
}

inline lazy_link_io&& operator&&(lazy_link_io&& lhs, lazy_link_io&& rhs) noexcept {
    set_link_link_io(lhs);
    return static_cast<lazy_link_io&&>(rhs);
}

inline void lazy_link_io::await_suspend(std::coroutine_handle<> current) const noexcept {
    this->last_io->io_info_.handle = current;
}

inline int32_t lazy_link_io::await_resume() const noexcept {
    return this->last_io->io_info_.result;
}

//  moves data between two file descriptors without copying
//  between kernel address space and user address space.  It transfers
//  up to size bytes of data from the file descriptor fd_in to the
//  file descriptor fd_out, where one of the file descriptors must
//  refer to a pipe.
struct lazy_splice : lazy_awaiter {
    inline lazy_splice(
        int fd_in,
        int64_t off_in,
        int fd_out,
        int64_t off_out,
        unsigned int nbytes,
        unsigned int splice_flags
    ) noexcept {
        io_uring_prep_splice(sqe_, fd_in, off_in, fd_out, off_out, nbytes, splice_flags);
    };
};

// duplicates up to size bytes of data from the pipe referred
// to by the file descriptor fd_in to the pipe referred to by the
// file descriptor fd_out.  It does not consume the data that is
// duplicated from fd_in; therefore, that data can be copied by a
// subsequent splice(2)
struct lazy_tee : lazy_awaiter {
    inline lazy_tee(
        int fd_in,
        int fd_out,
        unsigned int nbytes,
        unsigned int splice_flags
    ) noexcept {
        io_uring_prep_tee(sqe_, fd_in, fd_out, nbytes, splice_flags);
    }
};

// The readv() system call reads iovcnt buffers from the file
// associated with the file descriptor fd into the buffers described
// by iov ("scatter input").
struct lazy_readv : lazy_awaiter {
    inline lazy_readv(
        int fd,
        std::span<const iovec> iovecs,
        uint64_t offset
    ) noexcept {
        io_uring_prep_readv(sqe_, fd, iovecs.data(), iovecs.size(), offset);
    }
};

// 
struct lazy_readv2 : lazy_awaiter {
    inline lazy_readv2(
        int fd,
        std::span<const iovec> iovecs,
        uint64_t offset,
        int flags
    ) noexcept {
        io_uring_prep_readv2(sqe_, fd, iovecs.data(), iovecs.size(), offset, flags);
    }
};

struct lazy_read_fixed : lazy_awaiter {
    lazy_read_fixed(
        int fd,
        std::span<char> buf,
        uint64_t offset,
        uint16_t buf_index
    ) noexcept {
        io_uring_prep_read_fixed(sqe_, fd, buf.data(), buf.size(), offset, buf_index);
    }
};

} // coro::detail