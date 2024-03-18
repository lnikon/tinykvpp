//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEMOCKSEGMENT_H
#define ZKV_LSMTREEMOCKSEGMENT_H

#include <structures/lsmtree/segments/segment_interface.h>

namespace structures::lsmtree::segments::mock_segment
{

class mock_segment_t final : public interface::segment_interface_t
{
   public:
    explicit mock_segment_t(types::path_t path,
                            memtable::unique_ptr_t pMemtable) noexcept;

    [[nodiscard]] std::vector<std::optional<lsmtree::record_t>> record(
        const lsmtree::key_t &key) override;

    [[nodiscard]] std::optional<lsmtree::record_t> record(
        const hashindex::hashindex_t::offset_t &offset) override;

    types::name_t get_name() const override;
    types::path_t get_path() const override;

    void flush() override;
    ~mock_segment_t() noexcept override = default;
    void restore() override;
    memtable::unique_ptr_t memtable() override;
    std::filesystem::file_time_type last_write_time() override;

   private:
    memtable::unique_ptr_t m_pMemtable;
};

using shared_ptr_t = std::shared_ptr<mock_segment_t>;

template <typename... Args>
auto make_shared(Args... args)
{
    return std::make_shared<mock_segment_t>(std::forward<Args>(args)...);
}

}  // namespace structures::lsmtree::segments::mock_segment

#endif  // ZKV_LSMTREEMOCKSEGMENT_H
