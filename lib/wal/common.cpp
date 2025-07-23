#include "common.h"

namespace wal
{

[[nodiscard]] auto to_string(log_storage_type_k type) noexcept -> std::string_view
{
    if (type == log_storage_type_k::in_memory_k)
    {
        return std::string_view{"inMemory"};
    }

    if (type == log_storage_type_k::file_based_persistent_k)
    {
        return std::string_view{"persistent"};
    }

    return std::string_view{"undefined"};
}

[[nodiscard]] auto from_string(std::string_view type) noexcept -> log_storage_type_k
{
    if (type == "inMemory")
    {
        return log_storage_type_k::in_memory_k;
    }

    if (type == "persistent")
    {
        return log_storage_type_k::file_based_persistent_k;
    }

    return log_storage_type_k::undefined_k;
}

} // namespace wal
