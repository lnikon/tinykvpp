//
// Created by nikon on 2/6/22.
//

#pragma once

#include <filesystem>
#include <unordered_map>

#include <config/config.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/interface_lsmtree_segment.h>
#include <structures/lsmtree/segments/lsmtree_segment_storage.h>

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

  explicit lsmtree_segment_manager_t(const config::sptr_t &config,
                                     lsmtree::segment_storage::sptr pStorage);

  // TODO(lnikon): Should be thread-safe?
  segment_shared_ptr_t get_new_segment(const lsmtree_segment_type_t type,
                                       memtable_unique_ptr_t pMemtable);

  segment_shared_ptr_t get_segment(const segment_name_t &name);
  lsmtree::segment_storage::sptr get_segments();

  std::vector<segment_name_t> get_segment_names() const;
  std::vector<std::filesystem::path> get_segment_paths() const;

  // TODO: Start merging on-disk segments.
  // void compact();
private:
  std::string get_next_name();
  std::filesystem::path construct_path(const std::string &name) const;

private:
  config::sptr_t m_config;
  uint64_t m_index{0};
  segment_storage::sptr m_pStorage;
};

using lsmtree_segment_manager_shared_ptr_t =
    std::shared_ptr<lsmtree_segment_manager_t>;

template <typename... Args>
lsmtree_segment_manager_shared_ptr_t make_shared(Args... args) {
  return std::make_shared<lsmtree_segment_manager_t>(
      std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segment_manager
