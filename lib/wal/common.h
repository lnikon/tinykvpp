#pragma once

#include <cstdint>
#include <string_view>

namespace wal
{

enum class log_storage_type_k : int8_t
{
    undefined_k = -1,
    in_memory_k,
    file_based_persistent_k,
};

[[nodiscard]] auto to_string(log_storage_type_k type) noexcept -> std::string_view;
[[nodiscard]] auto from_string(std::string_view type) noexcept -> log_storage_type_k;

} // namespace wal
