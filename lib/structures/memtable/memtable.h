//
// Created by nikon on 1/21/22.
//

#ifndef MEMTABLE_H
#define MEMTABLE_H

#include <boost/date_time.hpp>

#define FMT_HEADER_ONLY
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "structures/sorted_vector/sorted_vector.h"

namespace structures::memtable {
using namespace sorted_vector;
// TODO: Maybe instead of supporting any K/V type, we can support fixed set of
// types?
// TODO: Use `concepts` to describe interface of template parameters.
// TODO: MemTable should be safe for concurrent access and *scalable*. Consider
// to use SkipList! template <template<typename> class StorageType>

std::size_t string_size_in_bytes(const std::string &str);

class memtable_t {
public:
  struct record_t {
    // TODO(vahag): Introduce 'buffer' type, an uninterpreted array of bytes
    enum class record_value_type_t { integer_k = 0, double_k, string_k };

    struct key_t {
			using storage_type_t = std::string;

      explicit key_t(std::string key);

      key_t(const key_t &other);
      key_t &operator=(const key_t &other);
      // TODO: Impl move assign operator

      bool operator<(const key_t &other) const;
      bool operator>(const key_t &other) const;
      bool operator==(const key_t &other) const;

      void write(std::stringstream &os) const;
      [[nodiscard]] std::size_t size() const;

      static void swap(key_t &lhs, key_t &rhs);

      storage_type_t m_key;
    };

    struct value_t {
      using underlying_value_type_t =
          std::variant<int64_t, double, std::string>;

      explicit value_t(underlying_value_type_t value);

      value_t(const value_t &other);
      value_t &operator=(const value_t &other);
      // TODO: Impl move assign operator

      bool operator==(const value_t &other) const;

      void write(std::stringstream &os) const;
      [[nodiscard]] std::optional<std::size_t> size() const;

      static void swap(value_t &lhs, value_t &rhs);

      underlying_value_type_t m_value;
    };

    record_t(const record_t &other);
    record_t(const key_t &key, const value_t &value);

    bool operator<(const record_t &record) const;
    bool operator>(const record_t &record) const;
    record_t &operator=(const record_t &other);

    [[nodiscard]] std::size_t size() const;

    friend std::ostream &operator<<(std::ostream &out, const record_t &r) {
      std::stringstream ss;
      r.m_key.write(ss);
      r.m_value.write(ss);
      ss << std::endl;
      out << ss.str();
      return out;
    }

    key_t m_key;
    value_t m_value;
  };

  using storage_t = sorted_vector_t<record_t>;

  memtable_t() = default;
  memtable_t(const memtable_t &) = delete;
  memtable_t &operator=(const memtable_t &) = delete;
  memtable_t(memtable_t &&) = delete;
  memtable_t &operator=(memtable_t &&) = delete;

  void emplace(const record_t &record);
  std::optional<record_t> find(const record_t::key_t &key);
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::size_t count() const;

  // TODO: Implement iterators to use for dumping
  // Consider using std::iterator
  // https://en.cppreference.com/w/cpp/iterator/iterator
  // Memtable is essentially a key -> value maaping, so
  // it should provide a universal begin/end iterators
  // which will point to pair and be available for structural destruction
  typename storage_t::iterator begin();
  typename storage_t::iterator end();

  void write(std::stringstream &ss);

private:
  void update_size(const record_t &record);

private:
  std::mutex m_mutex;
  // TODO: Should abstract out this part to some generic storage with good O(n)
  // times.
  storage_t m_data;
  std::size_t m_size{0};
  std::size_t m_count{0};
};

using unique_ptr_t = std::unique_ptr<memtable_t>;

template <typename... Args> auto make_unique(Args... args) {
  return std::make_unique<memtable_t>(std::forward<args>...);
}
} // namespace structures::memtable

template <>
struct std::hash<structures::memtable::memtable_t::record_t::key_t> {
  using S = structures::memtable::memtable_t::record_t::key_t;
  std::size_t operator()(const S &s) const {
    return std::hash<std::string>{}(s.m_key);
  }
};

#endif // MEMTABLE_H
