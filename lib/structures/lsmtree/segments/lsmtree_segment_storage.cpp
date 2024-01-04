#include <cassert>

#include <structures/lsmtree/segments/lsmtree_segment_storage.h>

namespace structures::lsmtree::segment_storage {

segment_shared_ptr_t
lsmtree_segment_storage_t::get(const name_type_t &name) const {
  assert(!name.empty());

  std::lock_guard lg(m_mutex);
  if (auto it = m_segmentsMap.find(name); it != m_segmentsMap.end()) {
    return it->second;
  }

  return nullptr;
}

void lsmtree_segment_storage_t::put(segment_shared_ptr_t pSegment) {
  assert(pSegment);

  std::lock_guard lg(m_mutex);
  if (auto it = m_segmentsMap.find(pSegment->get_name());
      it == m_segmentsMap.end()) {
    m_segmentsMap.emplace(pSegment->get_name(), pSegment);
    m_segmentsVector.emplace(
        pSegment, [](segment_shared_ptr_t lhs, segment_shared_ptr_t rhs) {
          return std::filesystem::last_write_time(lhs->get_path()) <
                 std::filesystem::last_write_time(rhs->get_path());
        });
  }
}

void lsmtree_segment_storage_t::remove(const name_type_t &name) {
  assert(!name.empty());

  std::lock_guard lg(m_mutex);
  if (auto it = m_segmentsMap.find(name); it == m_segmentsMap.end()) {
    m_segmentsMap.erase(name);
    m_segmentsVector.erase(std::remove_if(m_segmentsVector.begin(), m_segmentsVector.end(),
                              [&name](auto pSegment) {
                                return pSegment->get_name() == name;
                              }),
               m_segmentsVector.end());
  }
}

} // namespace structures::lsmtree::segment_storage
