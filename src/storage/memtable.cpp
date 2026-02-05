#include "storage/memtable.h"

bool frankie::storage::memtable_put(memtable* memtable, std::string key,
                                    std::string value) noexcept {
  return memtable->table_.emplace(key, value).second;
}

bool frankie::storage::memtable_get(memtable* memtable, const std::string& key,
                                    std::string& value) noexcept {
  if (const auto it = memtable->table_.find(key);
      it != memtable->table_.end()) {
    value = it->second;
    return true;
  }
  return false;
}

