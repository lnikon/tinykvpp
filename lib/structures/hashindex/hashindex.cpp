#include <structures/hashindex/hashindex.h>

namespace structures::hashindex {

hashindex_t::hashindex_t() : m_cursor{0} {}

void hashindex_t::emplace(structures::lsmtree::key_t key) {
  m_offsets.emplace(key, m_cursor);
  m_cursor += key.size();
}

bool hashindex_t::empty() const { return m_offsets.empty(); }

std::optional<hashindex_t::offset_t>
hashindex_t::get_offset(const structures::lsmtree::key_t &key) const {
  const auto it{m_offsets.find(key)};
  return it == m_offsets.end() ? std::nullopt : std::make_optional(it->second);
}

} // namespace structures::hashindex
