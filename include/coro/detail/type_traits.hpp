#pragma once

// #include <type_traits>
#include <utility>

namespace coro::detail {

template<typename T>
struct remove_rvalue_reference {
    using type = T;
};

template<typename T>
struct remove_rvalue_reference<T &&> {
    using type = T;
};

template<typename T>
using remove_rvalue_reference_t = remove_rvalue_reference<T>::type;

template<typename Awaiter>
using get_awaiter_result_t = decltype(std::declval<Awaiter>().await_resume());
} // coro::detail