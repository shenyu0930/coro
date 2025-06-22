#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 512
#define QUEUE_DEPTH 1

int main() {
    struct io_uring ring;
    int ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring init failed: %s\n", strerror(-ret));
        return 1;
    }

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Cannot get SQE\n");
        io_uring_queue_exit(&ring);
        return 1;
    }

    char buffer[BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    io_uring_prep_read(sqe, STDIN_FILENO, buffer, sizeof(buffer), 0);
    sqe->user_data = 6;  // 自定义标识符

    ret = io_uring_submit(&ring);
    if (ret < 1) {
        fprintf(stderr, "Submit failed: %d\n", ret);
        io_uring_queue_exit(&ring);
        return 1;
    }

    printf("Waiting for input... (type something and press Enter)\n");

    struct io_uring_cqe *cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        fprintf(stderr, "Wait CQE failed: %d\n", ret);
        io_uring_queue_exit(&ring);
        return 1;
    }

    if (cqe->res < 0) {
        fprintf(stderr, "Async read failed: %s\n", strerror(-cqe->res));
    } else {
        printf("Success! Read %d bytes:\n", cqe->res);
        printf("-> %.*s, user_data[%llu]\n", cqe->res, buffer, cqe->user_data);
    }

    io_uring_cqe_seen(&ring, cqe);

    io_uring_queue_exit(&ring);

    return 0;
}
