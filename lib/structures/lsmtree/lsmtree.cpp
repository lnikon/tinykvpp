#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/segment_interface.h>

namespace structures::lsmtree
{

lsmtree_t::lsmtree_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_pTable{memtable::make_unique()},
      m_levels{pConfig}
{
}

void lsmtree_t::put(const structures::lsmtree::key_t &key,
                    const structures::lsmtree::value_t &value) noexcept
{
    assert(m_pTable);
    assert(m_pConfig);

    // Add record into memtable
    m_pTable->emplace(record_t{key, value});

    // Check whether after addition size of the memtable increased above the
    // threashold. If so flush the memtable
    if (m_pTable->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        m_levels.segment(m_pConfig->LSMTreeConfig.SegmentType,
                         std::move(m_pTable));
        m_pTable = memtable::make_unique();
    }
}

std::optional<record_t> lsmtree_t::get(const key_t &key) const noexcept
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
    auto result{m_pTable->find(key)};

    // If key isn't in in-memory table, then it probably was flushed.
    // Lookup for the key in on-disk segments
    if (!result.has_value())
    {
        result = m_levels.record(key);
    }

    return result;
}

}  // namespace structures::lsmtree
