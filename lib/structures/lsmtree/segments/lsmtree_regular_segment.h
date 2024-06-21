//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEREGULARSEGMENT_H
#define ZKV_LSMTREEREGULARSEGMENT_H

#include <structures/lsmtree/segments/segment_interface.h>
#include <structures/lsmtree/segments/types.h>

namespace structures::lsmtree::segments::regular_segment
{

namespace types = lsmtree::segments::types;

class regular_segment_t final : public segments::interface::segment_interface_t
{
  public:
    /**
     * @brief
     *
     * @param path
     * @param name
     * @param memtable
     */
    regular_segment_t(types::path_t path, types::name_t name, memtable::memtable_t memtable);

    /**
     * @brief
     */
    virtual ~regular_segment_t() noexcept = default;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] std::vector<std::optional<lsmtree::record_t>> record(const lsmtree::key_t &key) override;

    /**
     * @brief
     *
     * @param offset
     */
    [[nodiscard]] std::optional<lsmtree::record_t> record(const hashindex::hashindex_t::offset_t &offset) override;

    /**
     * @brief
     */
    types::name_t get_name() const override;

    /**
     * @brief
     */
    types::path_t get_path() const override;

    /**
     * @brief
     */
    std::optional<memtable::memtable_t> memtable() override;

    /**
     * @brief
     */
    void restore() override;

    /**
     * @brief
     */
    void flush() override;

        /**
     * @brief
     */
    void purge() override;

    /**
     * @brief
     */
    std::filesystem::file_time_type last_write_time() override;

    /**
     * @brief
     */
    [[nodiscard]] virtual std::optional<record_t::key_t> min() const noexcept override;

    /**
     * @brief
     */
    [[nodiscard]] virtual std::optional<record_t::key_t> max() const noexcept override;

  private:
    const types::path_t m_path;
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
