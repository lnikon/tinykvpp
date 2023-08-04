#include "memtable.h"

namespace structures::memtable {
std::size_t string_size_in_bytes(const std::string &str) {
  return sizeof(std::string::value_type) * str.size();
}

memtable_t::record_t::key_t::key_t(std::string key) : m_key(std::move(key)) {}

memtable_t::record_t::key_t::key_t(const memtable_t::record_t::key_t &other)
    : m_key(other.m_key) {}

memtable_t::record_t::key_t &memtable_t::record_t::key_t::operator=(
    const memtable_t::record_t::key_t &other) {
  if (this == &other) {
    return *this;
  }

  key_t tmp(other);
  swap(*this, tmp);

  return *this;
}

void memtable_t::record_t::key_t::swap(memtable_t::record_t::key_t &lhs,
                                       memtable_t::record_t::key_t &rhs) {
  using std::swap;
  std::swap(lhs.m_key, rhs.m_key);
}

std::size_t memtable_t::record_t::key_t::Size() const {
  return string_size_in_bytes(m_key);
}

bool memtable_t::record_t::key_t::operator<(
    const memtable_t::record_t::key_t &other) const {
  return m_key < other.m_key;
}

bool memtable_t::record_t::key_t::operator>(
    const memtable_t::record_t::key_t &other) const {
  return !(*this < other);
}

bool memtable_t::record_t::key_t::operator==(
    const memtable_t::record_t::key_t &other) const {
  return m_key == other.m_key;
}

void memtable_t::record_t::key_t::Write(std::stringstream &os) const {
  os << m_key;
}

memtable_t::record_t::value_t::value_t(
    memtable_t::record_t::value_t::underlying_value_type_t value)
    : m_value(std::move(value)) {}

memtable_t::record_t::value_t::value_t(
    const memtable_t::record_t::value_t &other)
    : m_value(other.m_value) {}

memtable_t::record_t::value_t &memtable_t::record_t::value_t::operator=(
    const memtable_t::record_t::value_t &other) {
  if (this == &other) {
    return *this;
  }

  value_t tmp(other);
  std::swap(m_value, tmp.m_value);

  return *this;
}

std::optional<std::size_t> memtable_t::record_t::value_t::Size() const {
  return std::visit(
      [](const underlying_value_type_t &value) {
        if (value.index() == static_cast<std::size_t>(ValueType::Integer)) {
          return std::optional<std::size_t>(std::in_place, sizeof(int64_t));
        } else if (value.index() ==
                   static_cast<std::size_t>(ValueType::Double)) {
          return std::optional<std::size_t>(std::in_place, sizeof(double));
        } else if (value.index() ==
                   static_cast<std::size_t>(ValueType::String)) {
          return std::optional<std::size_t>(
              std::in_place,
              string_size_in_bytes(std::get<std::string>(value)));
        } else {
          spdlog::warn("Unsupported value type with value index=" +
                       std::to_string(value.index()));
          return std::optional<std::size_t>(std::in_place);
        }
      },
      m_value);
}

bool memtable_t::record_t::value_t::operator==(
    const memtable_t::record_t::value_t &other) const {
  return m_value == other.m_value;
}

void memtable_t::record_t::value_t::swap(memtable_t::record_t::value_t &lhs,
                                         memtable_t::record_t::value_t &rhs) {
  using std::swap;
  swap(lhs.m_value, rhs.m_value);
}

void memtable_t::record_t::value_t::Write(std::stringstream &os) {
  std::visit(
      [&os](const underlying_value_type_t &value) {
        if (value.index() == static_cast<std::size_t>(ValueType::Integer)) {
          os << std::get<static_cast<std::size_t>(ValueType::Integer)>(value);
        } else if (value.index() ==
                   static_cast<std::size_t>(ValueType::Double)) {
          os << std::get<static_cast<std::size_t>(ValueType::Double)>(value);
        } else if (value.index() ==
                   static_cast<std::size_t>(ValueType::String)) {
          os << std::get<static_cast<std::size_t>(ValueType::String)>(value);
        } else {
          spdlog::warn("Unsupported value type with value index=" +
                       std::to_string(value.index()));
        }
      },
      m_value);
}

memtable_t::record_t::record_t(const memtable_t::record_t::key_t &key,
                               const memtable_t::record_t::value_t &value)
    : m_key(key), m_value(value) {}

memtable_t::record_t::record_t(const memtable_t::record_t &other)
    : m_key(other.m_key), m_value(other.m_value) {}

memtable_t::record_t &
memtable_t::record_t::operator=(const memtable_t::record_t &other) {
  if (this == &other) {
    return *this;
  }

  record_t tmp(other);
  record_t::key_t::swap(m_key, tmp.m_key);
  record_t::value_t::swap(m_value, tmp.m_value);

  return *this;
}

bool memtable_t::record_t::operator<(const memtable_t::record_t &record) const {
  return m_key < record.m_key;
}

bool memtable_t::record_t::operator>(const memtable_t::record_t &record) const {
  return !(m_key < record.m_key);
}

memtable_t::record_t::key_t memtable_t::record_t::GetKey() const {
  return m_key;
}

memtable_t::record_t::value_t memtable_t::record_t::GetValue() const {
  return m_value;
}

std::size_t memtable_t::record_t::Size() const {
  const auto valueSizeOpt = m_value.Size();
  if (!valueSizeOpt.has_value()) {
    spdlog::warn("Value has null size!");
  }

  return m_key.Size() + valueSizeOpt.value_or(0);
}

void memtable_t::Emplace(const memtable_t::record_t &record) {
  std::lock_guard lg(m_mutex);
  m_data.emplace(record);

  update_size(record);
  m_count++;
}

std::optional<memtable_t::record_t>
memtable_t::Find(const memtable_t::record_t::key_t &key) {
  record_t record{key, record_t::value_t{""}};

  std::lock_guard lg(m_mutex);
  auto it = m_data.find(record);
  std::cout << m_data.at(it.second);
  return (it.first ? std::make_optional(m_data.at(it.second)) : std::nullopt);
}

std::size_t memtable_t::Size() const { return m_size; }

std::size_t memtable_t::Count() const { return m_count; }

auto memtable_t::begin() { return m_data.begin(); }

auto memtable_t::end() { return m_data.end(); }

void memtable_t::write(std::stringstream &ss) {
  std::lock_guard lg(m_mutex);
  if (m_count == 0) {
    spdlog::warn("trying to Write() empty table");
    return;
  }

  ss << m_count;
  for (const auto &record : m_data) {
    record.GetKey().Write(ss);
    ss << ' ';
    record.GetValue().Write(ss);
    ss << '\n';
  }
}

void memtable_t::update_size(const memtable_t::record_t &record) {
  m_size += record.Size();
}
} // namespace structures::memtable
