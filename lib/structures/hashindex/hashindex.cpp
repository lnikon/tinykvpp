#include "structures/hashindex/hashindex.h"

namespace structures::hashindex
{

auto hashindex_t::begin() -> hashindex_t::iterator
{
    return m_offsets.begin();
}

auto hashindex_t::end() -> hashindex_t::iterator
{
    return m_offsets.end();
}

void hashindex_t::emplace(record_t record, const std::size_t length)
{
    m_offsets.emplace(record.m_key, length);
    m_num_of_bytes += record.size() + length;
}

auto hashindex_t::empty() const -> bool
{
    return m_offsets.empty();
}

auto hashindex_t::offset(const key_t &key) const -> std::vector<hashindex_t::offset_t>
{
    std::vector<offset_t> result;
    for (auto offsets{m_offsets.equal_range(key)}; offsets.first != offsets.second; ++offsets.first)
    {
        result.emplace_back(offsets.first->second);
    }
    return result;
}

auto hashindex_t::num_of_bytes_used() const noexcept -> std::size_t
{
    return m_num_of_bytes;
}

} // namespace structures::hashindex
