#pragma once

#include <optional>
#include <string>

#include "storage/skiplist.hpp"

namespace frankie::storage {

struct memtable {
  skiplist* sl{nullptr};
};

memtable* create_memtable() noexcept;

bool memtable_put(memtable* memtable, std::string key,
                  std::string value) noexcept;

std::optional<std::string> memtable_get(memtable* memtable,
                                        const std::string& key) noexcept;

}  // namespace frankie::storage
