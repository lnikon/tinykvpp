#include "common.h"
#include <string_view>

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

    if (type == log_storage_type_k::replicated_log_storage_k)
    {
        return std::string_view{"replicated"};
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

    if (type == "replicated")
    {
        return log_storage_type_k::replicated_log_storage_k;
    }

    return log_storage_type_k::undefined_k;
}

} // namespace wal
