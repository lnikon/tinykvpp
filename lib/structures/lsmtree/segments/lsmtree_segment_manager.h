//
// Created by nikon on 2/6/22.
//

#pragma once

#include <config/config.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/segment_interface.h>
#include <structures/lsmtree/segments/segment_storage.h>

#include <filesystem>
#include <unordered_map>

namespace structures::lsmtree::segment_manager
{

/**
 * @class lsmtree_segment_manager_t
 * @brief Does the management of on-disk segments. On-disk segment is born
 * when memtable is flushed onto the disk.
 */
class lsmtree_segment_manager_t
{
   public:
    using segment_name_t = std::string;
    using segment_map_t =
        std::unordered_map<segment_name_t, segment_shared_ptr_t>;

    explicit lsmtree_segment_manager_t(
        const config::shared_ptr_t &config,
        lsmtree::segments::storage::shared_ptr_t pStorage);

    segments::interface::segment_interface_t get_new_segment(
        const lsmtree_segment_type_t type,
        memtable_unique_ptr_t pMemtable);

    segments::interface::segment_interface_t get_segment(
        const segment_name_t &name);
    
    lsmtree::segments::storage::shared_ptr_t get_segments();

    std::vector<segment_name_t> get_segment_names() const;
    std::vector<std::filesystem::path> get_segment_paths() const;

    // TODO: Start merging on-disk segments.
    // void compact();
   private:
    std::string get_next_name();
    std::filesystem::path construct_path(const std::string &name) const;

   private:
    config::shared_ptr_t m_config;
    uint64_t m_index{0};
    segments::storage::shared_ptr_t m_pStorage;
};

using lsmtree_segment_manager_shared_ptr_t =
    std::shared_ptr<lsmtree_segment_manager_t>;

template <typename... Args>
lsmtree_segment_manager_shared_ptr_t make_shared(Args... args)
{
    return std::make_shared<lsmtree_segment_manager_t>(
        std::forward<Args>(args)...);
}

}  // namespace structures::lsmtree::segment_manager
