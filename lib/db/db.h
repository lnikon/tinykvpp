#pragma once

#include <config/config.h>
#include <db/db_config.h>
#include <structures/lsmtree/lsmtree.h>

namespace db
{

using namespace structures::lsmtree;

// Directory structure:
// -- dbroot
//  -- segments
//		-- segment_1.sst
//		-- segment_2.sst
//		-- ...
//		-- segment_N.sst

class db_t
{
  public:
    explicit db_t(const config::shared_ptr_t config);

    [[nodiscard]] bool open();

    void put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value);
    std::optional<record_t> get(const structures::lsmtree::key_t &key);

  private:
    bool prepare_directory_structure();

  private:
    const config::shared_ptr_t m_config;
    structures::lsmtree::lsmtree_t m_lsmTree;
};

} // namespace db
