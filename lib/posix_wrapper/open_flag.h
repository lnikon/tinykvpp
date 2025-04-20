#pragma once

#include <fcntl.h>

#include <type_traits>

/*
 * Using a name `posix_wrapper` instead of `posix`, as it is reserved.
 * A set of more-or-less typesafe wrappers around POSIX flags, functions, etc...
 * Is not complete, and probably will never be. Instead, will be extended as
 * needed.
 **/
namespace posix_wrapper
{

enum class open_flag_k : int
{
    kReadOnly = O_RDONLY,
    kWriteOnly = O_WRONLY,
    kReadWrite = O_RDWR,
    kAppend = O_APPEND,
    kCreate = O_CREAT,
    kTruncate = O_TRUNC,
    kExclusive = O_EXCL,
    kNonBlock = O_NONBLOCK,
    kSync = O_SYNC,
    kDirect = O_DIRECT,
};

[[nodiscard]] constexpr auto operator|(open_flag_k lhs, open_flag_k rhs) noexcept -> open_flag_k
{
    using U = std::underlying_type_t<open_flag_k>;
    return static_cast<open_flag_k>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

[[nodiscard]] constexpr auto operator&(open_flag_k lhs, open_flag_k rhs) noexcept -> open_flag_k
{
    using U = std::underlying_type_t<open_flag_k>;
    return static_cast<open_flag_k>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

[[nodiscard]] constexpr auto operator|=(open_flag_k &lhs, open_flag_k rhs) noexcept -> open_flag_k
{
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr auto any(open_flag_k f) noexcept -> bool
{
    using U = std::underlying_type_t<open_flag_k>;
    return static_cast<U>(f) != 0;
}

[[nodiscard]] constexpr auto to_native(open_flag_k f) noexcept
{
    return static_cast<std::underlying_type_t<open_flag_k>>(f);
}

} // namespace posix_wrapper
