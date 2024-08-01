#include "memtable.h"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace structures::memtable
{

std::size_t string_size_in_bytes(const std::string &str)
{
    return sizeof(std::string::value_type) * str.size();
}

// ----------------------------------------------
// memtable_t::record_t::key_t
// ----------------------------------------------------
memtable_t::record_t::key_t::key_t(record_t::key_t::storage_type_t key)
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
    return m_key.size();
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
// memtable_t::record_t::value_t
// ------------------------------------------------
memtable_t::record_t::value_t::value_t(memtable_t::record_t::value_t::storage_type_t value)
    : m_value(std::move(value))
{
}

std::size_t memtable_t::record_t::value_t::size() const
{
    return m_value.size();
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
// memtable_t::record_t::timestamp_t
// ----------------------------------------------------
memtable_t::record_t::timestamp_t::timestamp_t()
    : m_value{clock_t::now()}
{
}

bool memtable_t::record_t::timestamp_t::operator<(const timestamp_t &other) const
{
    return m_value < other.m_value;
}

void memtable_t::record_t::timestamp_t::swap(timestamp_t &lhs, timestamp_t &rhs)
{
    using std::swap;
    swap(lhs.m_value, rhs.m_value);
}

// ---------------------------------------
// memtable_t::record_t
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
    record_t::timestamp_t::swap(m_timestamp, tmp.m_timestamp);

    return *this;
}

bool memtable_t::record_t::operator<(const memtable_t::record_t &record) const
{
    // return m_key < record.m_key && m_timestamp < record.m_timestamp;
    return (m_key != record.m_key) ? m_key < record.m_key : !(m_timestamp < record.m_timestamp);
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
// memtable_t
// -----------------------------
void memtable_t::emplace(const memtable_t::record_t &record)
{
    m_data.emplace(record);

    update_size(record);
    m_count++;
}

std::optional<memtable_t::record_t> memtable_t::find(const memtable_t::record_t::key_t &key)
{
    return m_data.find(key);
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
    return m_data.size() > 0 ? std::make_optional(m_data.cbegin()->m_key) : std::nullopt;
}

std::optional<memtable_t::record_t::key_t> memtable_t::max() const noexcept
{
    // TODO(lnikon): Fix this
    storage_t::const_iterator beforeEnd{nullptr};
    auto idx{0};
    for (auto begin{m_data.cbegin()}; begin != m_data.cend(); ++begin)
    {
        if (idx == 0)
        {
            continue;
        }
        else
        {
            beforeEnd = begin;
        }
    }
    return m_data.size() > 0 ? std::make_optional((beforeEnd)->m_key) : std::nullopt;
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
