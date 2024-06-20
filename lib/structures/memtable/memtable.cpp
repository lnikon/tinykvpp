#include "memtable.h"

#include <optional>
#include <utility>

namespace structures::memtable
{

std::size_t string_size_in_bytes(const std::string &str)
{
    return sizeof(std::string::value_type) * str.size();
}

// ----------------------------------------------
// Implementation for memtable_t::record_t::key_t
// ----------------------------------------------------
memtable_t::record_t::key_t::key_t(std::string key)
    : m_key(std::move(key))
{
}

void memtable_t::record_t::key_t::swap(memtable_t::record_t::key_t &lhs, memtable_t::record_t::key_t &rhs)
{
    using std::swap;
    std::swap(lhs.m_key, rhs.m_key);
}

std::size_t memtable_t::record_t::key_t::size() const
{
    return string_size_in_bytes(m_key);
}

bool memtable_t::record_t::key_t::operator<(const memtable_t::record_t::key_t &other) const
{
    return m_key < other.m_key;
}

bool memtable_t::record_t::key_t::operator>(const memtable_t::record_t::key_t &other) const
{
    return m_key > other.m_key;
}

bool memtable_t::record_t::key_t::operator==(const memtable_t::record_t::key_t &other) const
{
    return m_key == other.m_key;
}

// ------------------------------------------------
// Implementation for memtable_t::record_t::value_t
// ------------------------------------------------
memtable_t::record_t::value_t::value_t(memtable_t::record_t::value_t::underlying_value_type_t value)
    : m_value(std::move(value))
{
}

std::size_t memtable_t::record_t::value_t::size() const
{
    return std::visit(
        [](auto &&value)
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>)
            {
                return sizeof(value);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                return sizeof(value);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return string_size_in_bytes(value);
            }
            else
            {
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
                return std::size_t{0};
            }
        },
        m_value);
}

bool memtable_t::record_t::value_t::operator==(const memtable_t::record_t::value_t &other) const
{
    return m_value == other.m_value;
}

void memtable_t::record_t::value_t::swap(memtable_t::record_t::value_t &lhs, memtable_t::record_t::value_t &rhs)
{
    using std::swap;
    swap(lhs.m_value, rhs.m_value);
}

// ----------------------------------------------------
// Implementation for memtable_t::record_t::timestamp_t
// ----------------------------------------------------
memtable_t::record_t::timestamp_t::timestamp_t()
    : m_value{clock_t::now()}
{
}

bool memtable_t::record_t::timestamp_t::operator<(const timestamp_t &other) const
{
    return m_value < other.m_value;
}

// ---------------------------------------
// Implementation for memtable_t::record_t
// ---------------------------------------
memtable_t::record_t::record_t(const memtable_t::record_t::key_t &key, const memtable_t::record_t::value_t &value)
    : m_key(key),
      m_value(value)
{
}

memtable_t::record_t::record_t(const memtable_t::record_t &other)
    : m_key(other.m_key),
      m_value(other.m_value)
{
}

memtable_t::record_t &memtable_t::record_t::operator=(const memtable_t::record_t &other)
{
    if (this == &other)
    {
        return *this;
    }

    record_t tmp(other);
    record_t::key_t::swap(m_key, tmp.m_key);
    record_t::value_t::swap(m_value, tmp.m_value);

    return *this;
}

bool memtable_t::record_t::operator<(const memtable_t::record_t &record) const
{
    return m_key < record.m_key && m_timestamp < record.m_timestamp;
}

bool memtable_t::record_t::operator>(const memtable_t::record_t &record) const
{
    return !(m_key < record.m_key);
}

bool memtable_t::record_t::operator==(const record_t &record) const
{
    return m_key == record.m_key;
}

std::size_t memtable_t::record_t::size() const
{
    return m_key.size() + m_value.size();
}

// -----------------------------
// Implementation for memtable_t
// -----------------------------
void memtable_t::emplace(const memtable_t::record_t &record)
{
    m_data.emplace(record);

    update_size(record);
    m_count++;
}

std::optional<memtable_t::record_t> memtable_t::find(const memtable_t::record_t::key_t &key)
{
    record_t record{key, record_t::value_t{""}};

    auto it = m_data.find(record);
    return (it.first ? std::make_optional(m_data.at(it.second)) : std::nullopt);
}

std::size_t memtable_t::size() const
{
    return m_size;
}

std::size_t memtable_t::count() const
{
    return m_count;
}

[[nodiscard]] bool memtable_t::empty() const
{
    return m_data.size() == 0;
}

void memtable_t::merge(memtable_t pMemtable) noexcept
{
    // TODO: Use timestamp to compare items
    for (auto &record : pMemtable.m_data)
    {
        emplace(record);
    }
}

typename memtable_t::storage_t::const_iterator memtable_t::begin() const
{
    return m_data.cbegin();
}

typename memtable_t::storage_t::const_iterator memtable_t::end() const
{
    return m_data.cend();
}

std::optional<memtable_t::record_t::key_t> memtable_t::min() const noexcept
{
    static_assert(std::is_same_v<structures::sorted_vector::sorted_vector_t<record_t>, decltype(m_data)>);
    return m_data.size() > 0 ? std::make_optional(m_data.cbegin()->m_key) : std::nullopt;
}

std::optional<memtable_t::record_t::key_t> memtable_t::max() const noexcept
{
    static_assert(std::is_same_v<structures::sorted_vector::sorted_vector_t<record_t>, decltype(m_data)>);
    return m_data.size() > 0 ? std::make_optional(m_data.cend()->m_key) : std::nullopt;
}

bool memtable_t::operator<(const memtable_t &other)
{
    return max() < other.min();
}

void memtable_t::update_size(const memtable_t::record_t &record)
{
    m_size += record.size();
}

} // namespace structures::memtable
