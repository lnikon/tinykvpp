#include "storage/memtable.hpp"

#include "storage/skiplist.hpp"

namespace frankie::storage {

memtable* create_memtable() noexcept {
  memtable* mt = new memtable;
  mt->sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  return mt;
}

bool memtable_put(memtable* mt, std::string key, std::string value) noexcept {
  return skiplist_insert(mt->sl, key, value) == nullptr;
}

std::optional<std::string> memtable_get(memtable* memtable,
                                        const std::string& key) noexcept {
  skiplist_node* node = skiplist_search(memtable->sl, key);
  if (node != nullptr) {
    return node->value_;
  }
  return std::nullopt;
}

}  // namespace frankie::storage
