//
// Created by nikon on 2/6/22.
//

#pragma once

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/interface_lsmtree_segment.h>

#include <filesystem>
#include <unordered_map>

namespace structures::lsmtree::segment_manager {
/**
 * Does the management of on-disk segments.
 * On-disk segment is born when memtable is flushed onto the disk.
 */
class lsmtree_segment_manager_t {
public:
  using segment_name_t = std::string;
  using segment_map_t =
      std::unordered_map<segment_name_t, segment_shared_ptr_t>;

  explicit lsmtree_segment_manager_t(const std::filesystem::path &dbPath);

  // TODO(lnikon): Should be thread-safe?
  segment_shared_ptr_t get_new_segment(const lsmtree_segment_type_t type,
                                       memtable_unique_ptr_t pMemtable);

  segment_shared_ptr_t get_segment(const segment_name_t &name);

  std::vector<segment_name_t> get_segment_names() const;

  // TODO: Start merging on-disk segments.
  // void Compact();
private:
  std::string get_next_name();
	std::filesystem::path construct_path(const std::string& name) const;

private:
  std::filesystem::path m_dbPath;
  uint64_t m_index{0};
  segment_map_t m_segments;
};

using lsmtree_segment_manager_shared_ptr_t =
    std::shared_ptr<lsmtree_segment_manager_t>;

template <typename... Args>
lsmtree_segment_manager_shared_ptr_t make_shared(Args... args) {
  return std::make_shared<lsmtree_segment_manager_t>(std::forward(args)...);
}

} // namespace structures::lsmtree::segment_manager
