#pragma once

#include <config/config.h>
#include <db/db_config.h>
#include <structures/lsmtree/lsmtree.h>

namespace db {

using namespace structures::lsmtree;

// Directory structure:
// -- dbroot
//  -- segments
//		-- segment_1.sst
//		-- segment_2.sst
//		-- ...
//		-- segment_N.sst

class db_t {
public:
  using segstorage_sptr_t = lsmtree::segment_storage::sptr;
  using segmgr_sptr_t = segment_manager::lsmtree_segment_manager_shared_ptr_t;
  using lsmtree_config_t = structures::lsmtree::lsmtree_config_t;
  using lsmtree_t = structures::lsmtree::lsmtree_t;

  explicit db_t(const config::sptr_t config);

  bool open();

  void put(const structures::lsmtree::key_t &key,
           const structures::lsmtree::value_t &value);

  std::optional<record_t> get(const structures::lsmtree::key_t &key) const;

private:
  // TODO(lnikon): Prepare directory structure for the database when opening a
  // DB
  bool prepare_directory_structure();

private:
  const config::sptr_t m_config;
  segstorage_sptr_t m_pSegmentStorage;
  segmgr_sptr_t m_pSegmentManager;
  lsmtree_t m_lsmTree;
};

} // namespace db
