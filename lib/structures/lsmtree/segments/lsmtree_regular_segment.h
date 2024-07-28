//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEREGULARSEGMENT_H
#define ZKV_LSMTREEREGULARSEGMENT_H

#include "fs/append_only_file.h"
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
     */
    virtual ~regular_segment_t() noexcept = default;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] std::vector<std::optional<memtable::memtable_t::record_t>> record(const lsmtree::key_t &key);

    /**
     * @brief
     *
     * @param offset
     */
    [[nodiscard]] std::optional<memtable::memtable_t::record_t> record(const hashindex::hashindex_t::offset_t &offset);

    /**
     * @brief
     */
    types::name_t get_name() const;

    /**
     * @brief
     */
    types::path_t get_path() const;

    /**
     * @brief
     */
    std::optional<memtable::memtable_t> memtable();

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
    void purge();

    /**
     * @brief
     */
    std::filesystem::file_time_type last_write_time();

    /**
     * @brief
     */
    [[nodiscard]] virtual std::optional<memtable::memtable_t::record_t::key_t> min() const noexcept;

    /**
     * @brief
     */
    [[nodiscard]] virtual std::optional<memtable::memtable_t::record_t::key_t> max() const noexcept;

  private:
    void restore_index();

  private:
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
