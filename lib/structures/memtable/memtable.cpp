#include "memtable.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace structures::memtable
{

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

auto memtable_t::record_t::key_t::size() const -> std::size_t
{
    return m_key.size();
}

auto memtable_t::record_t::key_t::operator<(const memtable_t::record_t::key_t &other) const -> bool
{
    return m_key < other.m_key;
}

auto memtable_t::record_t::key_t::operator>(const memtable_t::record_t::key_t &other) const -> bool
{
    return m_key > other.m_key;
}

auto memtable_t::record_t::key_t::operator==(const memtable_t::record_t::key_t &other) const -> bool
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

auto memtable_t::record_t::value_t::size() const -> std::size_t
{
    return m_value.size();
}

auto memtable_t::record_t::value_t::operator==(const memtable_t::record_t::value_t &other) const -> bool
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

auto memtable_t::record_t::timestamp_t::operator<(const timestamp_t &other) const -> bool
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
memtable_t::record_t::record_t(memtable_t::record_t::key_t key, memtable_t::record_t::value_t value)
    : m_key(std::move(key)),
      m_value(std::move(value))
{
}

memtable_t::record_t::record_t(const memtable_t::record_t &other)
    : m_key(other.m_key),
      m_value(other.m_value)
{
}

auto memtable_t::record_t::operator=(const memtable_t::record_t &other) -> memtable_t::record_t &
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

auto memtable_t::record_t::operator<(const memtable_t::record_t &record) const -> bool
{
    // return m_key < record.m_key && m_timestamp < record.m_timestamp;
    return (m_key != record.m_key) ? m_key < record.m_key : !(m_timestamp < record.m_timestamp);
}

auto memtable_t::record_t::operator>(const memtable_t::record_t &record) const -> bool
{
    return !(m_key < record.m_key);
}

auto memtable_t::record_t::operator==(const record_t &record) const -> bool
{
    return m_key == record.m_key;
}

auto memtable_t::record_t::size() const -> std::size_t
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

auto memtable_t::find(const memtable_t::record_t::key_t &key) -> std::optional<memtable_t::record_t>
{
    return m_data.find(key);
}

auto memtable_t::size() const -> std::size_t
{
    return m_size;
}

auto memtable_t::count() const -> std::size_t
{
    return m_count;
}

[[nodiscard]] auto memtable_t::empty() const -> bool
{
    return m_data.size() == 0;
}

void memtable_t::merge(memtable_t pMemtable) noexcept
{
    // TODO(nikon): Use timestamp to compare items
    for (auto &record : pMemtable.m_data)
    {
        emplace(record);
    }
}

auto memtable_t::begin() const -> typename memtable_t::storage_t::const_iterator
{
    return m_data.cbegin();
}

auto memtable_t::end() const -> typename memtable_t::storage_t::const_iterator
{
    return m_data.cend();
}

auto memtable_t::min() const noexcept -> std::optional<memtable_t::record_t::key_t>
{
    return m_data.size() > 0 ? std::make_optional(m_data.cbegin()->m_key) : std::nullopt;
}

auto memtable_t::max() const noexcept -> std::optional<memtable_t::record_t::key_t>
{
    // TODO(lnikon): Fix this
    storage_t::const_iterator beforeEnd{nullptr};
    auto idx{0};
    for (auto begin{m_data.cbegin()}; begin != m_data.cend(); ++begin)
    {
        if (idx++ == 0)
        {
            continue;
        }

        beforeEnd = begin;
    }
    return m_data.size() > 0 ? std::make_optional((beforeEnd)->m_key) : std::nullopt;
}

[[nodiscard]] auto memtable_t::moved_records() -> std::vector<memtable_t::record_t>
{
    return {std::make_move_iterator(std::begin(m_data)), std::make_move_iterator(std::end(m_data))};
}

auto memtable_t::operator<(const memtable_t &other) const -> bool
{
    return max() < other.min();
}

void memtable_t::update_size(const memtable_t::record_t &record)
{
    m_size += record.size();
}

} // namespace structures::memtable
