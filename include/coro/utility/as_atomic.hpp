#pragma once

#include <memory>
#include <atomic>

namespace coro {

template<typename T>
requires std::is_trivially_copyable_v<T>
#if 0 // __cplusplus >= 202002L
inline std::atomic_ref<T>& as_atomic(T& value) noexcept {
    return std::atomic_ref<T>(value);
}
#else
inline std::atomic<T>& as_atomic(T& value) noexcept {
    return *reinterpret_cast<std::atomic<T>*>(std::addressof(value));
}
#endif

template<typename T>
requires std::is_trivially_copyable_v<T>
#if 0 //__cplusplus >= 202002L
inline const std::atomic_ref<T>& as_const_atomic(const T& value) noexcept {
    return std::atomic_ref<T>(value);
}
#else
inline const std::atomic<T>& as_const_atomic(const T& value) noexcept {
    return *reinterpret_cast<std::atomic<T>*>(std::addressof(value));
}
#endif

} // coro