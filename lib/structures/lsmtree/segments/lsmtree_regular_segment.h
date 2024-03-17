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
    regular_segment_t(types::path_t path,
                      types::name_t name,
                      memtable::unique_ptr_t pMemtable);

    virtual ~regular_segment_t() noexcept = default;

    [[nodiscard]] std::vector<std::optional<lsmtree::record_t>> record(
        const lsmtree::key_t &key) override;

    [[nodiscard]] std::optional<lsmtree::record_t> record(
        const hashindex::hashindex_t::offset_t &offset) override;

    types::name_t get_name() const override;
    types::path_t get_path() const override;
    memtable::unique_ptr_t memtable() override;
    void restore() override;
    void flush() override;
    std::filesystem::file_time_type last_write_time() override;

   private:
    const types::path_t m_path;
    const types::name_t m_name;
    hashindex::hashindex_t m_hashIndex;
    memtable::unique_ptr_t m_pMemtable;
};

using shared_ptr_t = std::shared_ptr<regular_segment_t>;

template <typename... Args>
auto make_shared(Args... args)
{
    return std::make_shared<regular_segment_t>(std::forward<Args>(args)...);
}

}  // namespace structures::lsmtree::segments::regular_segment

#endif  // ZKV_LSMTREEREGULARSEGMENT_H
