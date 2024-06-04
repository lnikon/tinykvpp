#ifndef LSM_TREE_SEGMENT_STORAGE
#define LSM_TREE_SEGMENT_STORAGE

#include <structures/lsmtree/segments/segment_interface.h>
#include <structures/lsmtree/segments/types.h>
#include <structures/sorted_vector/sorted_vector.h>

#include <memory>
#include <unordered_map>

namespace structures::lsmtree::segments::storage
{

namespace types = lsmtree::segments::types;
namespace segment = lsmtree::segments::interface;

/**
 * @class last_write_time_comparator_t
 * @brief Use to insert a segment into level0
 *
 */
struct last_write_time_comparator_t
{
    bool operator()(segment::shared_ptr_t lhs, segment::shared_ptr_t rhs)
    {
        return lhs->last_write_time() <= rhs->last_write_time();
    }
};

class segment_storage_t : public std::enable_shared_from_this<segment_storage_t>
{
  public:
    using name_t = types::name_t;
    using segment_map_t = std::unordered_map<name_t, segment::shared_ptr_t>;
    using segment_comp_t = std::function<bool(segment::shared_ptr_t, segment::shared_ptr_t)>;
    using storage_t = structures::sorted_vector::sorted_vector_t<segment::shared_ptr_t, segment_comp_t>;
    using iterator = storage_t::iterator;
    using const_iterator = storage_t::const_iterator;
    using reverse_iterator = storage_t::reverse_iterator;
    using size_type = storage_t::size_type;

    [[nodiscard]] size_type size() const noexcept;

    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;

    [[nodiscard]] const_iterator cbegin() const noexcept;
    [[nodiscard]] const_iterator cend() const noexcept;

    [[nodiscard]] reverse_iterator rbegin() noexcept;
    [[nodiscard]] reverse_iterator rend() noexcept;

    void emplace(segment::shared_ptr_t pSegment, segment_comp_t comp);
    void clear() noexcept;

  private:
    mutable std::mutex m_mutex; // TODO(lnikon): Use clang's mutex borrow checker
    segment_map_t m_segmentsMap;
    storage_t m_segmentsVector;
};

using shared_ptr_t = std::shared_ptr<segment_storage_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<segment_storage_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segments::storage

#endif // LSM_TREE_SEGMENT_STORAGE
