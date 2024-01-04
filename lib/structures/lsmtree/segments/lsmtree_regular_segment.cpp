//
// Created by nikon on 2/6/22.
//

#include "structures/lsmtree/lsmtree_types.h"
#include <optional>
#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <cassert>
#include <fstream>

#include <structures/lsmtree/segments/lsmtree_regular_segment.h>

// WriteableSegment -> get_record
// ReadableSegment -> flush
// or:
// enum SegmentMode { Read, Write, Read | Write }

namespace structures::lsmtree {

lsmtree_regular_segment_t::lsmtree_regular_segment_t(
    std::filesystem::path path, memtable_unique_ptr_t pMemtable)
    : interface_lsmtree_segment_t(std::move(path), std::move(pMemtable)) {}

[[nodiscard]] std::optional<lsmtree::record_t>
lsmtree_regular_segment_t::get_record(const lsmtree::key_t &key) {
  // TODO(lnikon): Check `prepopulate_segment_index`. If set, skip building the
  // index.

  assert(!m_hashIndex.empty());

  const auto offset{m_hashIndex.get_offset(key)};
  if (offset) {
    // TODO(lnikon): Consider memory mapping
    std::fstream segmentStream{get_path(), std::ios::in};
    segmentStream.seekg(offset.value());
    // TODO(lnikon): Check that offset isn't greater than the stream
    std::size_t keySz{0};
    lsmtree::record_t::key_t::storage_type_t key;
    std::size_t valueSz{0};
    std::string valueStr;
    lsmtree::record_t::value_t::underlying_value_type_t value;

    segmentStream >> keySz;
    segmentStream >> key;
    segmentStream >> valueSz;
    segmentStream >> valueStr;
    value = valueStr;

    spdlog::info("keySz={} key={} valueSz={} valueStr={}", keySz, key, valueSz,
                  valueStr);

    return std::make_optional(
        record_t{key_t{std::move(key)}, value_t{std::move(value)}});
  }

  return std::nullopt;
}

void lsmtree_regular_segment_t::flush() {
  // If m_pMemtable is null, then segment has been flushed
  if (!m_pMemtable) {
    return;
  }

  std::stringstream ss;
  for (const auto &kv : *m_pMemtable) {
    ss << kv;
    m_hashIndex.emplace(kv.m_key);
  }

  // TODO(vahag): Use fadvise() and O_DIRECT
  // TODO(vahag): Async IO?
  std::fstream stream(get_path(), std::fstream::out | std::fstream::app);
  if (!stream.is_open()) {
    // TODO(vahag): How to handle situation when it's impossible to flush
    // memtable into disk?
    spdlog::error("(lsmtree_regular_segment_t): unable to flush regular "
                  "segment with path=" +
                  get_path().string() + "\n");
    return;
  }

  stream << ss.str();
  stream.flush();
  stream.close();

  // TODO(lnikon): On successfull flush purge the old delete memtable
}

} // namespace structures::lsmtree
