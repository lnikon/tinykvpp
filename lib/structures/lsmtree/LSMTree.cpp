//
// Created by nikon on 1/21/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include "LSMTree.h"

namespace structures::lsmtree {

enum class EStatus {
    Undefined,
    Ok,
    AccessDenied,
};

LSMTree::LSMTree(const LSMTreeConfig &config)
    : m_config(config), m_table(memtable::make_unique()),
      m_segmentsMgr(std::make_shared<LSMTreeSegmentManager>()) {}

void LSMTree::Put(const structures::lsmtree::key_t &key,
                  const structures::lsmtree::value_t &value) {
  // TODO: Wrap into check optionalValueSize and use everywhere
  const auto valueSizeOpt = value.size();
  if (!valueSizeOpt.has_value()) {
    spdlog::warn("Value size is null! Refusing to insert!");
    return;
  }

  // TODO: Is it possible to use better locking strategy?
  std::unique_lock ul(m_mutex);
  const auto newSize = key.size() + valueSizeOpt.value() + m_table->size();
  if (newSize >= m_config.DiskFlushThresholdSize) {
    spdlog::debug("flushing table. m_table->size()={}", m_table->size());
    // TODO: For now, lock whole table, dump it into on-disk segment, and
    // replace the table with new one.
    // TODO: For the future, keep LSMTree readable while dumping.
    // This can be achieved by moving the current copy into the new async task to dump and creating a new copy.
    auto tableToDump = std::move(m_table);
    m_table = memtable::make_unique();

    std::stringstream ss;
    tableToDump->write(ss);

    auto segment = m_segmentsMgr->GetNewSegment(m_config.SegmentType);
    segment->SetContent(ss.str());
    segment->Flush();
  }

  m_table->emplace(record_t{key, value});
}

std::optional<record_t> structures::lsmtree::LSMTree::Get(const key_t &key) const {
  // TODO: Use Index and on-disk segments.
  // For now we'll just lookup in-memory memtable.

  // First, search in in-memory table.
  auto result = m_table->find(key);

  // If can't find in in-memory table, then lookup on-disk segments.
  if (!result) {
    // TODO: Implement.
    return std::nullopt;
  }

  return result;
}

} // namespace structures::lsmtree
