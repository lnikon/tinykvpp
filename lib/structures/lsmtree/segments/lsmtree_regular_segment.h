//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEREGULARSEGMENT_H
#define ZKV_LSMTREEREGULARSEGMENT_H

#include "fs/types.h"
#include "structures/memtable/memtable.h"
#include "structures/hashindex/hashindex.h"
#include <structures/lsmtree/segments/types.h>

namespace structures::lsmtree::segments::regular_segment
{

namespace types = lsmtree::segments::types;

class regular_segment_t final
{
  public:
    /**
     * @brief
     *
     * @param path
     * @param name
     * @param memtable
     */
    regular_segment_t(fs::path_t path, types::name_t name, memtable::memtable_t memtable);

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] auto record(const lsmtree::key_t &key) -> std::vector<std::optional<memtable::memtable_t::record_t>>;

    /**
     * @brief
     *
     * @param offset
     */
    [[nodiscard]] auto record(const hashindex::hashindex_t::offset_t &offset)
        -> std::optional<memtable::memtable_t::record_t>;

    /**
     * @brief
     */
    [[nodiscard]] auto get_name() const -> types::name_t;

    /**
     * @brief
     */
    [[nodiscard]] auto get_path() const -> types::path_t;

    /**
     * @brief
     */
    auto memtable() -> std::optional<memtable::memtable_t> &;

    /**
     * @brief
     */
    auto moved_memtable() -> std::optional<memtable::memtable_t>;

    /**
     * @brief
     */
    void restore();

    /**
     * @brief
     */
    void flush();

    /**
     * @brief
     */
    void remove_from_disk() const noexcept;

    /**
     * @brief
     */
    auto last_write_time() -> std::filesystem::file_time_type;

    /**
     * @brief
     */
    [[nodiscard]] auto min() const noexcept -> std::optional<memtable::memtable_t::record_t::key_t>;

    /**
     * @brief
     */
    [[nodiscard]] auto max() const noexcept -> std::optional<memtable::memtable_t::record_t::key_t>;

    [[nodiscard]] auto num_of_bytes_used() const -> std::size_t;

  private:
    void restore_index();

    const fs::path_t m_path;
    const types::name_t m_name;

    hashindex::hashindex_t m_hashIndex;
    std::optional<memtable::memtable_t> m_memtable;

    std::optional<memtable::memtable_t::record_t::key_t> m_minKey;
    std::optional<memtable::memtable_t::record_t::key_t> m_maxKey;
};

using shared_ptr_t = std::shared_ptr<regular_segment_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<regular_segment_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segments::regular_segment

#endif // ZKV_LSMTREEREGULARSEGMENT_H
