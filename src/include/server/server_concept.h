#pragma once

#include <concepts>

#include "db/db.h"

namespace server
{

template <typename T>
concept communication_strategy_t = requires(T strategy, db::shared_ptr_t db) {
    { strategy.start(db) } -> std::same_as<void>;
    { strategy.shutdown() } -> std::same_as<void>;
};

} // namespace server
