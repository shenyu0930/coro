#pragma once
#include <cstdint>

namespace coro::detail {

enum class reserved_user_data : uint64_t {
    nop,
    none,
};

enum class user_data_type : uint8_t {
    task_info_ptr,
    coroutine_handle,
    task_info_ptr_link_sqe,
    msg_ring,
    none,
};

} // coro::detail