#pragma once

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include <functional>
#include <structures/sorted_vector/sorted_vector.h>

#include <absl/synchronization/mutex.h>

#include <memory>
#include <unordered_map>

namespace structures::lsmtree::segments::storage
{

namespace types = lsmtree::segments::types;

/**
 * @class last_write_time_comparator_t
 * @brief Use to insert a segment into level0
 *
 */
struct last_write_time_comparator_t
{
    auto operator()(const regular_segment::shared_ptr_t &lhs,
                    const regular_segment::shared_ptr_t &rhs) -> bool
    {
        return lhs->last_write_time() <= rhs->last_write_time();
    }
};

/**
 * @class key_range_comparator_t
 * @brief
 *
 */
struct key_range_comparator_t
{
    auto operator()(const regular_segment::shared_ptr_t &lhs,
                    const regular_segment::shared_ptr_t &rhs) -> bool
    {
        return lhs->max() < rhs->min();
    }
};

/**
 * @class segment_storage_t
 * @brief
 *
 */
class segment_storage_t : public std::enable_shared_from_this<segment_storage_t>
{
  public:
    using name_t = types::name_t;
    using segment_map_t = std::unordered_map<name_t, regular_segment::shared_ptr_t>;
    using segment_comp_t =
        std::function<bool(regular_segment::shared_ptr_t, regular_segment::shared_ptr_t)>;
    using storage_t =
        structures::sorted_vector::sorted_vector_t<regular_segment::shared_ptr_t, segment_comp_t>;
    using iterator = storage_t::iterator;
    using const_iterator = storage_t::const_iterator;
    using reverse_iterator = storage_t::reverse_iterator;
    using size_type = storage_t::size_type;

    [[nodiscard]] auto size() const noexcept -> size_type;

    [[nodiscard]] auto begin() noexcept -> iterator;
    [[nodiscard]] auto end() noexcept -> iterator;

    [[nodiscard]] auto begin() const noexcept -> const_iterator;
    [[nodiscard]] auto end() const noexcept -> const_iterator;

    [[nodiscard]] auto cbegin() const noexcept -> const_iterator;
    [[nodiscard]] auto cend() const noexcept -> const_iterator;

    [[nodiscard]] auto rbegin() noexcept -> reverse_iterator;
    [[nodiscard]] auto rend() noexcept -> reverse_iterator;

    void emplace(regular_segment::shared_ptr_t pSegment, segment_comp_t comp);
    void clear() noexcept;
    void remove(regular_segment::shared_ptr_t pSegment);
    auto find(const std::string &name) const noexcept -> regular_segment::shared_ptr_t;

  private:
    // TODO(lnikon): Use thread annotations
    mutable absl::Mutex m_mutex;
    // TODO(lnikon): Use std::set to keep segment names
    segment_map_t m_segmentsMap;
    storage_t     m_segmentsVector;
};

using shared_ptr_t = std::shared_ptr<segment_storage_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<segment_storage_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segments::storage
