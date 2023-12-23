//
// Created by nikon on 1/21/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/interface_lsmtree_segment.h>

namespace structures::lsmtree {

lsmtree_t::lsmtree_t(const config::sptr_t config,
                     lsmtree_segment_manager_shared_ptr_t pSegmentsMgr)
    : m_config(config), m_table(memtable::make_unique()),
      m_pSegmentsMgr(pSegmentsMgr) {}

// TODO (vahag): Update WAL
void lsmtree_t::put(const structures::lsmtree::key_t &key,
                    const structures::lsmtree::value_t &value) {
  // TODO: Wrap into check optionalValueSize and use everywhere
  const auto valueSizeOpt = value.size();
  if (!valueSizeOpt.has_value()) {
    spdlog::warn("Value size is null! Refusing to insert!");
    return;
  }

  // TODO(lnikon): Is it possible to use better locking strategy?
  // TODO(lnikon): Compactation can be a periodic job?
  if (std::unique_lock ul(m_mutex);
      key.size() + valueSizeOpt.value() + m_table->size() >=
      m_config->LSMTreeConfig.DiskFlushThresholdSize) {
    spdlog::debug("flushing table. m_table->size()={}", m_table->size());

    // TODO(lnikon): For the future, keep LSMTree readable while dumping.
    // This can be achieved by moving the current copy into the new async task
    // to dump and creating a new copy.
    m_pSegmentsMgr
        ->get_new_segment(m_config->LSMTreeConfig.SegmentType,
                          std::move(m_table))
        ->flush();
    m_table = memtable::make_unique();
  }

  m_table->emplace(record_t{key, value});
}

std::optional<record_t>
structures::lsmtree::lsmtree_t::get(const key_t &key) const {
  // TODO(lnikon): Use Index and on-disk segments.
  // For now we'll just lookup in-memory memtable.
  auto result{m_table->find(key)};
  if (result) {
    return result;
  } else {
    const auto segments = m_pSegmentsMgr->get_segments();
    for (const auto &segment : *segments) {
      result = segment->get_record(key);
      if (result.has_value()) {
        return result;
      }
    }
  }

  return std::nullopt;
}

} // namespace structures::lsmtree
