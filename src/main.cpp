#include <cstdint>
#include <map>
#include <string>

namespace frankie {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace storage {

struct memtable {
  std::map<std::string, std::string> table_;
};

bool memtable_put(memtable* memtable, std::string key,
                  std::string value) noexcept {
  return memtable->table_.emplace(key, value).second;
}

bool memtable_get(memtable* memtable, const std::string& key,
                  std::string& value) noexcept {
  if (const auto it = memtable->table_.find(key);
      it != memtable->table_.end()) {
    value = it->second;
    return true;
  }
  return false;
}

}  // namespace storage

}  // namespace frankie

int main() {
  using namespace frankie;

  storage::memtable table;
  storage::memtable_put(&table, std::string{"key1"}, std::string{"value1"});
  storage::memtable_put(&table, std::string{"key2"}, std::string{"value2"});
  storage::memtable_put(&table, std::string{"key3"}, std::string{"value3"});

  return 0;
}
