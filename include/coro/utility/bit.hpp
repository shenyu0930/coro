#pragma once

#include <concepts>

namespace coro {

template<std::unsigned_integral T>
inline constexpr T bit_top = T(-1) ^ (T(-1) >> 1);

template<std::integral T>
inline constexpr T lowbit(T value) noexcept {
    return value & (-value);
}

} // coro