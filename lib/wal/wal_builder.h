#pragma once

#include "wal/wal.h"
#include "wal/common.h"
#include "wal/in_memory_log_storage.h"
#include "wal/persistent_log_storage.h"

#include <log_storage_builder.h>

namespace wal
{

using wal_variant_t = std::variant<wal::wal_t<log_t<in_memory_storage_t>>, wal::wal_t<log_t<persistent_log_storage_t>>>;

class wal_builder_t
{
  public:
    wal_builder_t() = default;

    template <typename... Args>
    [[nodiscard]] auto build(wal::log_storage_type_k type, Args... args) -> std::optional<wal_variant_t>
    {
        auto&& storage{buildStorage(type, args...)};
        if (!storage.has_value())
        {
            return std::nullopt;
        }

        if (type == wal::log_storage_type_k::in_memory_k)
        {
            return wal::wal_t(log_t<in_memory_log_storage_t>(std::move(storage.value())));
        }

        if (type == wal::log_storage_type_k::persistent_k)
        {
            return wal::wal_t(
                log_t<persistent_log_storage_t>(std::move(storage.value())));
        }

        return std::nullopt;
    }
};

} // namespace wal
