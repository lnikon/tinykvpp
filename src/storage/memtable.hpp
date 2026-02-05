#pragma once

#include <map>
#include <string>

namespace frankie::storage {

struct memtable {
  std::map<std::string, std::string> table_;
};

bool memtable_put(memtable* memtable, std::string key,
                  std::string value) noexcept;

bool memtable_get(memtable* memtable, const std::string& key,
                  std::string& value) noexcept;

}  // namespace frankie::storage
