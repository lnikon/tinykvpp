#pragma once

#include "db/db.h"

#include <concepts>

namespace server
{

template <typename T>
concept communication_strategy_t = requires(T strategy, db::db_t &db) {
    { strategy.start(db) } -> std::same_as<void>;
};

} // namespace server
