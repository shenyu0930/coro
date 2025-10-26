#pragma once

#include <functional>
#include <initializer_list>
#include <liburing.h>

#include "sqe_awaitable.hpp"
#include "task.hpp"
#include "utils.hpp"

#ifdef LIBURING_VERBOSE
#   define puts_if_verbose(x) puts(x)
#   define printf_if_verbose(...) printf(__VA_ARGS__)
#else
#   define puts_if_verbose(x) 0
#   define printf_if_verbose(...) 0
#endif

namespace coro {

class IOService {
public:
    IOService(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0) {
        io_uring_params p{
            .flags = flags,
            .wq_fd = wq_fd, // shared SQPOLL thread by rings
        };

        io_uring_queue_init_params(entries, &ring_, &p) | PanicOnErr("queue_init_params", false).use_errno;

        auto* probe = io_uring_get_probe_ring(&ring_);
        OnScopeExit free_probe([=]() { io_uring_free_probe(probe);} );

#define TEST_IORING_OP(opcode) do { \
    for (int i = 0; i < probe->ops_len; ++i) { \
        if (probe->ops[i].op == opcode && probe->ops[i].flags & IO_URING_OP_SUPPORTED) { \
            probe_ops_[i] = true; \
            break; \
        }\
    } \
}while(0)
    
    TEST_IORING_OP(IORING_OP_NOP);
    TEST_IORING_OP(IORING_OP_READV);
	TEST_IORING_OP(IORING_OP_WRITEV);
	TEST_IORING_OP(IORING_OP_FSYNC);
	TEST_IORING_OP(IORING_OP_READ_FIXED);
	TEST_IORING_OP(IORING_OP_WRITE_FIXED);
	TEST_IORING_OP(IORING_OP_POLL_ADD);
	TEST_IORING_OP(IORING_OP_POLL_REMOVE);
	TEST_IORING_OP(IORING_OP_SYNC_FILE_RANGE);
	TEST_IORING_OP(IORING_OP_SENDMSG);
	TEST_IORING_OP(IORING_OP_RECVMSG);
	TEST_IORING_OP(IORING_OP_TIMEOUT);
	TEST_IORING_OP(IORING_OP_TIMEOUT_REMOVE);
	TEST_IORING_OP(IORING_OP_ACCEPT);
	TEST_IORING_OP(IORING_OP_ASYNC_CANCEL);
	TEST_IORING_OP(IORING_OP_LINK_TIMEOUT);
	TEST_IORING_OP(IORING_OP_CONNECT);
	TEST_IORING_OP(IORING_OP_FALLOCATE);
	TEST_IORING_OP(IORING_OP_OPENAT);
	TEST_IORING_OP(IORING_OP_CLOSE);
	TEST_IORING_OP(IORING_OP_FILES_UPDATE);
	TEST_IORING_OP(IORING_OP_STATX);
	TEST_IORING_OP(IORING_OP_READ);
	TEST_IORING_OP(IORING_OP_WRITE);
	TEST_IORING_OP(IORING_OP_FADVISE);
	TEST_IORING_OP(IORING_OP_MADVISE);
	TEST_IORING_OP(IORING_OP_SEND);
	TEST_IORING_OP(IORING_OP_RECV);
	TEST_IORING_OP(IORING_OP_OPENAT2);
	TEST_IORING_OP(IORING_OP_EPOLL_CTL);
	TEST_IORING_OP(IORING_OP_SPLICE);
	TEST_IORING_OP(IORING_OP_PROVIDE_BUFFERS);
	TEST_IORING_OP(IORING_OP_REMOVE_BUFFERS);
	TEST_IORING_OP(IORING_OP_TEE);
	TEST_IORING_OP(IORING_OP_SHUTDOWN);
	TEST_IORING_OP(IORING_OP_RENAMEAT);
	TEST_IORING_OP(IORING_OP_UNLINKAT);
	TEST_IORING_OP(IORING_OP_MKDIRAT);
	TEST_IORING_OP(IORING_OP_SYMLINKAT);
	TEST_IORING_OP(IORING_OP_LINKAT);
	TEST_IORING_OP(IORING_OP_MSG_RING);
	TEST_IORING_OP(IORING_OP_FSETXATTR);
	TEST_IORING_OP(IORING_OP_SETXATTR);
	TEST_IORING_OP(IORING_OP_FGETXATTR);
	TEST_IORING_OP(IORING_OP_GETXATTR);
	TEST_IORING_OP(IORING_OP_SOCKET);
	TEST_IORING_OP(IORING_OP_URING_CMD);
	TEST_IORING_OP(IORING_OP_SEND_ZC);
	TEST_IORING_OP(IORING_OP_SENDMSG_ZC);
    }

	~IOService() noexcept {
		io_uring_queue_exit(&ring_);
	}

	IOService(const IOService&) = delete;
	IOService& operator=(const IOService&) = delete;
public:
	// read data into multiple buffers asynchronously
	SqeAwaitable readv(
		int fd,
		const iovec* iovecs,
		unsigned nr_vecs,
		off_t offset,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
		return AwaitWork(sqe, iflags);
	}

	SqeAwaitable readv2(
		int fd,
		const iovec* iovecs,
		unsigned nr_vecs,
		off_t offset,
		int flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_readv2(sqe, fd, iovecs, nr_vecs, offset, flags);
		return AwaitWork(sqe, iflags);
	}

	// write data into multiple buffers asynchronously
	SqeAwaitable writev(
		int fd,
		const iovec* iovecs,
		unsigned nr_vecs,
		off_t offset,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
		return AwaitWork(sqe, iflags);
	}

	SqeAwaitable writev2(
		int fd,
		const iovec* iovecs,
		unsigned nr_vecs,
		off_t offset,
		int flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_writev2(sqe, fd, iovecs, nr_vecs, offset, flags);
		return AwaitWork(sqe, iflags);
	}

	// read from a file descriptor at a given offset asynchronously
	SqeAwaitable read(
		int fd,
		void* buf,
		size_t nbytes,
		off_t offset,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_read(sqe, fd, buf, nbytes, offset);
		return AwaitWork(sqe, iflags);
	}

	// write data to a file descriptor at a given offset asynchronously
	SqeAwaitable write(
		int fd,
		const void* buf,
		size_t nbytes,
		off_t offset,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_write(sqe, fd, buf, nbytes, offset);
		return AwaitWork(sqe, iflags);
	}

	// read data into a fixed buffer asynchronously
	SqeAwaitable read_fixed(
		int fd,
		void* buf,
		unsigned nbytes,
		off_t offset,
		int buf_index,
		uint8_t iflags = 0
	) noexcept
	{
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_read_fixed(sqe, fd, buf, nbytes, offset, buf_index);
		return AwaitWork(sqe, iflags);
	}

	// write data into a fixed buffer asynchronously
	SqeAwaitable write_fixed(
		int fd,
		const void* buf,
		size_t nbytes,
		off_t offset,
		int buf_index,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_write_fixed(sqe, fd, buf, nbytes, offset, buf_index);
		return AwaitWork(sqe, iflags);
	}

	// synchronize a file's in-core state with storage device asynchronously
	SqeAwaitable fsync(
		int fd,
		int fsync_flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_fsync(sqe, fd, fsync_flags);
		return AwaitWork(sqe, iflags);
	}

	// synchronize a file segement with disk asynchronously
	SqeAwaitable sync_file_range(
		int fd,
		off64_t offset,
		off64_t nbytes,
		unsigned sync_range_flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_rw(IORING_OP_SYNC_FILE_RANGE, sqe, fd, nullptr, nbytes, sync_range_flags);
		return AwaitWork(sqe, iflags);
	}

	// receive a message from a socket asynchronously
	SqeAwaitable recvmsg(
		int sockfd,
		msghdr* msg,
		uint32_t flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_recvmsg(sqe, sockfd, msg, flags);
		return AwaitWork(sqe, iflags);
	}

	// send a message on a socket asynchronously
	SqeAwaitable sendmsg(
		int sockfd,
		const msghdr* msg,
		uint32_t flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_sendmsg(sqe, sockfd, msg, flags);
		return AwaitWork(sqe, iflags);
	}

	// receive a message from a socket asynchronously
	SqeAwaitable recv(
		int sockfd,
		void* buf,
		unsigned nbytes,
		uint32_t flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_recv(sqe, sockfd, buf, nbytes, flags);
		return AwaitWork(sqe, iflags);
	}

	// send a message on a socket asynchronously
	SqeAwaitable send(
		int sockfd,
		const void* buf,
		unsigned nbytes,
		uint32_t flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_send(sqe, sockfd, buf, nbytes, flags);
		return AwaitWork(sqe, iflags);
	}

	// wait for an event on a file descriptor asynchronously
	SqeAwaitable poll(
		int fd,
		short poll_mask,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_poll_add(sqe, fd, poll_mask);
		return AwaitWork(sqe, iflags);
	}

	// enqueue a noop command, which eventually acts like pthread_yield when awaiting
	SqeAwaitable yield(
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_nop(sqe);
		return AwaitWork(sqe, iflags);
	}

	// accept a connection on a socket asynchronously
	SqeAwaitable accept(
		int sockfd,
		sockaddr* addr,
		socklen_t* addrlen,
		int flags = 0,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_accept(sqe, sockfd, addr, addrlen, flags);
		return AwaitWork(sqe, iflags);
	}

	// initiate a connection on a socket asynchronously
	SqeAwaitable connect(
		int sockfd,
		const sockaddr* addr,
		socklen_t addrlen,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_connect(sqe, sockfd, addr, addrlen);
		return AwaitWork(sqe, iflags);
	}

	// wait for specified duration asynchronously
	SqeAwaitable timeout(
		__kernel_timespec *ts,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_timeout(sqe, ts, 0, 0);
		return AwaitWork(sqe, iflags);
	}

	// open and possibly create a file asynchronously
	SqeAwaitable open(
		int dfd,
		const char* path,
		int flags,
		mode_t mode = 0,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_openat(sqe, dfd, path, flags, mode);
		return AwaitWork(sqe, iflags);
	}

	// close a file descriptor asynchronously
	SqeAwaitable close(
		int fd,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_close(sqe, fd);
		return AwaitWork(sqe, iflags);
	}

	// get file status asynchronously
	SqeAwaitable stat(
		int dfd,
		const char *path,
		int flags,
		unsigned mask,
		struct statx* statxbuf,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_statx(sqe, dfd, path, flags, mask, statxbuf);
		return AwaitWork(sqe, iflags);
	}

	// splice data to/from a pipe asynchronously
	SqeAwaitable splice(
		int fd_in,
		loff_t off_in,
		int fd_out,
		loff_t off_out,
		size_t nbytes,
		unsigned flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_splice(sqe, fd_in, off_in, fd_out, off_out, nbytes, flags);
		return AwaitWork(sqe, iflags);
	}

	// duplicate pipe content asynchronously
	SqeAwaitable tee(
		int fd_in,
		int fd_out,
		size_t nbytes,
		unsigned flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_tee(sqe, fd_in, fd_out, nbytes, flags);
		return AwaitWork(sqe, iflags);
	}

	// shut down part of a full-duplex connection asynchronously
	SqeAwaitable shutdown(
		int sockfd,
		int how,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_shutdown(sqe, sockfd, how);
		return AwaitWork(sqe, iflags);
	}

	// change the name or location of a file asynchronously
	SqeAwaitable renameat(
		int olddfd,
		const char* oldpath,
		int newdfd,
		const char* newpath,
		int flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_renameat(sqe, olddfd, oldpath, newdfd, newpath, flags);
		return AwaitWork(sqe, iflags);
	}

	// create a directory asynchronously
	SqeAwaitable mkdirat(
		int dfd,
		const char* pathname,
		mode_t mode,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_mkdirat(sqe, dfd, pathname, mode);
		return AwaitWork(sqe, iflags);
	}

	// make a new name for a file asynchronously
	SqeAwaitable symlinkat(
		const char* target,
		int newdirfd,
		const char* linkpath,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_symlinkat(sqe, target, newdirfd, linkpath);
		return AwaitWork(sqe, iflags);
	}

	// make a new name for a file asynchronously
	SqeAwaitable linkat(
		int olddfd,
		const char* oldpath,
		int newdfd,
		const char* newpath,
		int flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_linkat(sqe, olddfd, oldpath, newdfd, newpath, flags);
		return AwaitWork(sqe, iflags);
	}

	// delete a name and possibly the file it refers to asynchronously
	SqeAwaitable unlinkat(
		int dfd,
		const char* pathname,
		int flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_unlinkat(sqe, dfd, pathname, flags);
		return AwaitWork(sqe, iflags);
	}

	SqeAwaitable msg_ring(
		int fd,
		unsigned len, 
		uint64_t data,
		unsigned flags,
		uint8_t iflags = 0
	) noexcept {
		auto* sqe = io_uring_get_sqe_safe();
		io_uring_prep_msg_ring(sqe, fd, len, data, flags);
		return AwaitWork(sqe, iflags);
	}
private:
	SqeAwaitable AwaitWork(
		io_uring_sqe* sqe,
		uint8_t iflags
	) noexcept {
		io_uring_sqe_set_flags(sqe, iflags);
		return SqeAwaitable{sqe};
	}

public:
	// get a sqe pointer that can never be null
	[[nodiscard]]
	io_uring_sqe* io_uring_get_sqe_safe() noexcept { 
		auto* sqe = io_uring_get_sqe(&ring_);
		if (!!sqe) [[likely]] {
			return sqe;
		} else {
			printf_if_verbose(__FILE__ ": SQ is full, flusing %u cqe(s)\n", cqe_count_);
			io_uring_cq_advance(&ring_, cqe_count_);
			cqe_count_ = 0;
			io_uring_submit(&ring_);
			sqe = io_uring_get_sqe(&ring_);

			if (!!sqe) [[likely]] {
				return sqe;
			} else {
				Panic("io_uring_get_sqe", ENOMEM);
			}
		}
	}

	// wait for an event forever, blocking
	template <typename T, bool nothrow>
	T run(const Task<T, nothrow>& t) noexcept(nothrow) {
		while (!t.done()) {
			io_uring_submit_and_wait(&ring_, 1);

			io_uring_cqe* cqe;
			unsigned head;

			io_uring_for_each_cqe(&ring_, head, cqe) {
				++cqe_count_;
				auto coro = static_cast<Resolver*>(io_uring_cqe_get_data(cqe));
				if (coro) {
					coro->resolve(cqe->res);
				}
			}

			printf_if_verbose(__FILE__, ": found %u cqe(s)\n, looping...\n", cqe_count);

			io_uring_cq_advance(&ring_, cqe_count_);
			cqe_count_ = 0;
		}

		return t.get_result();
	}

public:
	// register files for I/O
	void register_files(std::initializer_list<int> fds) {
		register_files(fds.begin(), (unsigned int)fds.size());
	}

	void register_files(const int *files, unsigned int nr_files) {
		io_uring_register_files(&ring_, files, nr_files) | PanicOnErr("io_uring_register_files", false);
	}

	// update registered files
	void register_files_update(unsigned off, int *files, unsigned nr_files) {
		io_uring_register_files_update(&ring_, off, files, nr_files) | PanicOnErr("io_uring_register_files", false);
	}

	// unregister files
	void register_files_unregister() {
		io_uring_unregister_files(&ring_);
	}

public:
	// register buffers for I/O
	template <unsigned int N>
	void register_buffers(iovec (&&ioves)[N]) {
		register_buffers(&ioves[0], N);
	}

	void register_buffers(const struct iovec *iovecs, unsigned nr_iovecs) {
		io_uring_register_buffers(&ring_, iovecs, nr_iovecs) | PanicOnErr("io_uring_register_buffers", false);
	}

	// unregister all buffers
	int unregister_buffers() {
		return io_uring_unregister_buffers(&ring_);
	}
public:
	// return internal io_uring_handle
	[[nodiscard]]
	io_uring& get_handle() noexcept {
		return ring_;
	}
private:
    io_uring ring_;
    unsigned cqe_count_{};
    bool probe_ops_[IORING_OP_LAST] = {};
};

}