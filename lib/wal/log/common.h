#pragma once

#include "in_memory_log_storage.h"
#include "persistent_log_storage.h"

#include <variant>

namespace wal::log
{

using log_storage_variant_t =
    std::variant<wal::log::in_memory_log_storage_t,
                 wal::log::persistent_log_storage_t<wal::log::file_storage_backend_t>>;

class log_storage_builder_t final
{
  public:
    [[nodiscard]] auto build() noexcept -> log_storage_variant_t
    {
    }
};

} // namespace wal::log
