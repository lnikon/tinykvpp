#pragma once

#include "db/db.h"

#include <concepts>

template <typename T>
concept CommunicationStrategy = requires(T strategy, db::db_t &db) {
    { strategy.start(db) } -> std::same_as<void>;
};
