#ifndef LSM_TREE_SEGMENT_STORAGE
#define LSM_TREE_SEGMENT_STORAGE

#include <memory>
#include <unordered_map>

#include <structures/lsmtree/segments/interface_lsmtree_segment.h>

namespace structures::lsmtree::segment_storage {

class lsmtree_segment_storage_t
    : public std::enable_shared_from_this<lsmtree_segment_storage_t> {
public:
  using name_type_t = interface_lsmtree_segment_t::name_t;
  using segment_sptr_vector = std::vector<segment_shared_ptr_t>;
  using segment_name_sptr_map =
      std::unordered_map<name_type_t, segment_shared_ptr_t>;

  using size_type = segment_sptr_vector::size_type;

  [[nodiscard]] segment_shared_ptr_t get(const name_type_t &name) const;
  void put(segment_shared_ptr_t pSegment);
  void remove(const name_type_t &name);
  [[nodiscard]] size_type size() const { return m_segmentsVector.size(); }

  // TODO(lnikon): Iterate in sorted order
  [[nodiscard]] inline auto begin() { return m_segmentsVector.begin(); }
  [[nodiscard]] inline auto end() { return m_segmentsVector.end(); }

private:
  mutable std::mutex m_mutex;
  segment_name_sptr_map m_segmentsMap;
  segment_sptr_vector m_segmentsVector;
};

using sptr = std::shared_ptr<lsmtree_segment_storage_t>;

template <typename... Args> sptr make_shared(Args... args) {
  return std::make_shared<lsmtree_segment_storage_t>(
      std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segment_storage

#endif // LSM_TREE_SEGMENT_STORAGE
