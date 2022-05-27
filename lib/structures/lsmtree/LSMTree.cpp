//
// Created by nikon on 1/21/22.
//

#include "LSMTree.h"

namespace structures::lsmtree {

LSMTree::LSMTree(const LSMTreeConfig &config)
    : m_config(config), m_table(memtable::make_unique()),
      m_segmentsMgr(std::make_shared<LSMTreeSegmentManager>()) {}

void LSMTree::Put(const structures::lsmtree::Key &key,
                  const structures::lsmtree::Value &value) {
  // TODO: Wrap into check optionalValueSize and use everywhere
  const auto valueSizeOpt = value.Size();
  if (!valueSizeOpt.has_value()) {
    spdlog::warn("Value size is null! Refusing to insert!");
    return;
  }

  std::unique_lock ul(m_mutex);
  const auto newSize = key.Size() + valueSizeOpt.value() + m_table->Size();
  if (newSize >= m_config.DiskFlushThresholdSize) {
    // TODO: For now, lock whole table, dump it into on-disk segment, and
    // replace the table with new one.
    // TODO: For the future, keep LSMTree readable while dumping.
    auto tableToDump = std::move(m_table);
    m_table = memtable::make_unique();

    std::stringstream ss;
    tableToDump->Write(ss);

    auto segment = m_segmentsMgr->GetNewSegment(m_config.SegmentType);
    segment->SetContent(ss.str());
    segment->Flush();
  }

  m_table->Emplace(Record{key, value});
}

void structures::lsmtree::LSMTree::Get() {
    assert(false);
}

} // namespace structures::lsmtree
