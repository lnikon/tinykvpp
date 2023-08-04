//
// Created by nikon on 1/21/22.
//

#ifndef CPP_PROJECT_TEMPLATE_MEMTABLE_H
#define CPP_PROJECT_TEMPLATE_MEMTABLE_H

#include <algorithm>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <boost/date_time.hpp>

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include "structures/sorted_vector/sorted_vector.h"

namespace structures::memtable {
using namespace sorted_vector;
// TODO: Maybe instead of supporting any K/V type, we can support fixed set of
// types?
// TODO: Use `concepts` to describe interface of template parameters.
// TODO: MemTable should be safe for concurrent access and *scalable*. Consider
// to use SkipList! template <template<typename> class StorageType>

std::size_t StringSizeInBytes(const std::string &str);

class memtable_t {
public:
  struct record_t {
    enum class ValueType { Integer = 0, Double, String };

    struct key_t {
      explicit key_t(std::string key);

      key_t(const key_t &other);
      key_t &operator=(const key_t &other);
      // TODO: Impl move assign operator

      bool operator<(const key_t &other) const;
      bool operator>(const key_t &other) const;
      bool operator==(const key_t &other) const;

      void Write(std::stringstream &os) const;
      [[nodiscard]] std::size_t Size() const;

      static void swap(key_t &lhs, key_t &rhs);

      std::string m_key;
    };

    struct value_t {
      using underlying_value_type_t = std::variant<int64_t, double, std::string>;

      explicit value_t(underlying_value_type_t value);

      value_t(const value_t &other);
      value_t &operator=(const value_t &other);
      // TODO: Impl move assign operator

      bool operator==(const value_t &other) const;

      void Write(std::stringstream &os);
      [[nodiscard]] std::optional<std::size_t> Size() const;

      static void swap(value_t &lhs, value_t &rhs);

      underlying_value_type_t m_value;
    };

    record_t(const record_t &other);
    record_t(const key_t &key, const value_t &value);

    bool operator<(const record_t &record) const;
    bool operator>(const record_t &record) const;
    record_t &operator=(const record_t &other);

    [[nodiscard]] key_t GetKey() const;
    [[nodiscard]] value_t GetValue() const;
    [[nodiscard]] std::size_t Size() const;

    friend std::ostream& operator<<(std::ostream &out, const record_t &r) {
      std::stringstream s;
      r.GetKey().Write(s);
      r.GetValue().Write(s);
      out << s.str();
      return out;
    }

  private:
    key_t m_key;
    value_t m_value;
  };

  memtable_t() = default;
  memtable_t(const memtable_t &) = delete;
  memtable_t &operator=(const memtable_t &) = delete;
  memtable_t(memtable_t &&) = delete;
  memtable_t &operator=(memtable_t &&) = delete;

  void Emplace(const record_t &record);
  std::optional<record_t> Find(const record_t::key_t &key);
  [[nodiscard]] std::size_t Size() const;
  [[nodiscard]] std::size_t Count() const;

  // TODO: Implement iterators to use for dumping
  // Consider using std::iterator
  // https://en.cppreference.com/w/cpp/iterator/iterator
  auto Begin();
  auto End();

  void Write(std::stringstream &ss);

private:
  void updateSize(const record_t &record);

private:
  std::mutex m_mutex;
  // TODO: Should abstract out this part to some generic storage with good O(n)
  // times.
  sorted_vector_t<record_t> m_data;
  std::size_t m_size{0};
  std::size_t m_count{0};
};

using MemTableUniquePtr = std::unique_ptr<memtable_t>;

template <typename... Args> auto make_unique(Args... args) {
  return std::make_unique<memtable_t>(std::forward<args>...);
}
} // namespace structures::memtable

#endif // CPP_PROJECT_TEMPLATE_MEMTABLE_H
