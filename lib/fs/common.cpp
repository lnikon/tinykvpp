#include "common.h"

#include <cstring>

namespace fs
{

[[nodiscard]] auto fs::file_error_t::has_error() const noexcept -> bool
{
    return code != file_error_code_k::none;
}

auto fs::file_error_t::from_errno(file_error_code_k code, int err, const char *context) noexcept
    -> file_error_t
{
    return {
        .code = code, .system_errno = err, .message = std::string(context) + ": " + strerror(err)};
}
auto fs::file_error_t::success() noexcept -> file_error_t
{
    return file_error_t{};
}

} // namespace fs