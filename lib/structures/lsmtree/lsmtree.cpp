#include <optional>
#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/segment_interface.h>

namespace structures::lsmtree
{

lsmtree_t::lsmtree_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_table{std::make_optional<memtable::memtable_t>()},
      m_levels{pConfig}
{
}

void lsmtree_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value) noexcept
{
    assert(m_pTable);
    assert(m_pConfig);

    // Add record into memtable
    m_table->emplace(record_t{key, value});

    // Check whether after addition size of the memtable increased above the
    // threashold. If so flush the memtable
    // TODO(lnikon): This logic should be a part of
    if (m_table->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        m_levels.segment(m_pConfig->LSMTreeConfig.SegmentType, std::move(m_table.value()));
        m_table = std::make_optional<memtable::memtable_t>();
    }
}

std::optional<record_t> lsmtree_t::get(const key_t &key) noexcept
{
    assert(m_pTable);

    // TODO(lnikon): Skip searching if record doesn't exist
    //    const auto recordExists{m_bloom.exists(key)};
    //    if (!recordExists)
    //    {
    //        return std::nullopt;
    //    }

    // If bloom check passed, then record probably exists.
    // Lookup in-memory table for the table
    auto result{m_table->find(key)};

    // If key isn't in in-memory table, then it probably was flushed.
    // Lookup for the key in on-disk segments
    if (!result.has_value())
    {
        result = m_levels.record(key);
    }

    return result;
}

bool lsmtree_t::restore() noexcept
{
    assert(m_pConfig);
    return false;
}

} // namespace structures::lsmtree
