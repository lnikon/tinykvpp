#include <structures/hashindex/hashindex.h>

namespace structures::hashindex {

hashindex_t::hashindex_t() : m_cursor{0} {}

void hashindex_t::emplace(structures::lsmtree::key_t key) {
  m_offsets.emplace(key, m_cursor);
  m_cursor += key.size();
}

} // namespace structures::hashindex
