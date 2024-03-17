#include <structures/hashindex/hashindex.h>

namespace structures::hashindex
{

hashindex_t::hashindex_t()
    : m_cursor{0}
{
}

void hashindex_t::emplace(lsmtree::record_t record, const std::size_t length)
{
    m_offsets.emplace(record.m_key, m_cursor);
    m_cursor += length;
}

bool hashindex_t::empty() const
{
    return m_offsets.empty();
}

std::vector<hashindex_t::offset_t> hashindex_t::offset(
    const lsmtree::key_t &key) const
{
    std::vector<offset_t> result;
    for (auto offsets{m_offsets.equal_range(key)};
         offsets.first != offsets.second;
         ++offsets.first)
    {
        result.emplace_back(offsets.first->second);
    }
    return result;
}

hashindex_t::iterator hashindex_t::begin()
{
    return m_offsets.begin();
}

hashindex_t::iterator hashindex_t::end()
{
    return m_offsets.end();
}

}  // namespace structures::hashindex
