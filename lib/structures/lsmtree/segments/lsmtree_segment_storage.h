#ifndef LSM_TREE_SEGMENT_STORAGE
#define LSM_TREE_SEGMENT_STORAGE

#include <memory>
#include <unordered_map>

#include <structures/lsmtree/segments/interface_lsmtree_segment.h>
#include <structures/sorted_vector/sorted_vector.h>

namespace structures::lsmtree::segment_storage {

// TODO(lnikon): Generic storage
// template <typename TStorageType, typename TStorageComparatorType>
class lsmtree_segment_storage_t
    : public std::enable_shared_from_this<lsmtree_segment_storage_t> {
public:
  using name_type_t = interface_lsmtree_segment_t::name_t;
  using segment_sptr_vector = std::vector<segment_shared_ptr_t>;
  using segment_name_sptr_map =
      std::unordered_map<name_type_t, segment_shared_ptr_t>;
  using segment_sptr_comp_t =
      std::function<bool(segment_shared_ptr_t, segment_shared_ptr_t)>;
  using storage_t =
      structures::sorted_vector::sorted_vector_t<segment_shared_ptr_t,
                                                 segment_sptr_comp_t>;

  // TODO(lnikon): Generic storage
  // using segment_sptr_comp_t = TStorageType;
  // using storage_t = TStorageComparatorType;

  using size_type = storage_t::size_type;

  [[nodiscard]] segment_shared_ptr_t get(const name_type_t &name) const;
  void put(segment_shared_ptr_t pSegment);
  void remove(const name_type_t &name);

  [[nodiscard]] inline auto size() const { return std::size(m_segmentsVector); }
  [[nodiscard]] inline auto begin() { return std::begin(m_segmentsVector); }
  [[nodiscard]] inline auto end() { return std::end(m_segmentsVector); }

private:
  mutable std::mutex m_mutex;
  segment_name_sptr_map m_segmentsMap;
  storage_t m_segmentsVector;
};

using sptr = std::shared_ptr<lsmtree_segment_storage_t>;

template <typename... Args> sptr make_shared(Args... args) {
  return std::make_shared<lsmtree_segment_storage_t>(
      std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segment_storage

#endif // LSM_TREE_SEGMENT_STORAGE
