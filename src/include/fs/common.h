#pragma once

#include <string>

namespace fs
{

/**
 * Default buffer size to use with io_uring
 */
static constexpr std::size_t kBufferSize{4096ULL};

/**
 * Default permissions to open a file with
 */
static constexpr int kDefaultFilePermissions = 0644;

/**
 * Default number of entries to use with io_uring queues
 */
static constexpr int kIOUringQueueEntries = 128;

enum class file_error_code_k : int8_t
{
    undefined = -1,
    none,
    open_failed,
    io_uring_init_failed,
    read_failed,
    write_failed,
    stat_failed,
    seek_failed,
    truncate_failed,
    flush_failed,
    invalid_file_descriptor,
    excl_file_exists,
};

struct file_error_t
{
    file_error_code_k code{file_error_code_k::none};
    int               system_errno{0};
    std::string       message;

    [[nodiscard]] auto has_error() const noexcept -> bool;

    [[nodiscard]] static auto success() noexcept -> file_error_t;
    [[nodiscard]] static auto
    from_errno(file_error_code_k code, int err, const char *context) noexcept -> file_error_t;
};

} // namespace fs
