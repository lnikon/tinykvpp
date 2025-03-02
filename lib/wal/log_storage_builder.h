#pragma once

#include "wal/common.h"
#include "wal/in_memory_log_storage.h"
#include "wal/persistent_log_storage.h"

using storage_variant_t = std::variant<in_memory_storage_t, persistent_log_storage_t>;

template <typename... Args>
[[nodiscard]] auto buildStorage(const wal::log_storage_type_k type, Args... args) -> std::optional<storage_variant_t>
{
    if (type == wal::log_storage_type_k::in_memory_k)
    {
        return in_memory_storage_builder_t{}.build();
    }

    if (type == wal::log_storage_type_k::persistent_k)
    {
        return persistent_log_storage_builder_t{args...}.build();
    }

    return std::nullopt;
}