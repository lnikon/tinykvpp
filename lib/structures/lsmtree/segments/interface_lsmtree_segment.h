//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_INTERFACE_LSMTREE_SEGMENT_H
#define ZKV_INTERFACE_LSMTREE_SEGMENT_H

#include <structures/hashindex/hashindex.h>
#include <structures/lsmtree/lsmtree_types.h>

#include <filesystem>
#include <memory>
#include <string>

namespace structures::lsmtree {

class interface_lsmtree_segment_t {
public:
  using name_t = std::string;
  using path_t = std::filesystem::path;

  explicit interface_lsmtree_segment_t(std::filesystem::path path,
                                       memtable_unique_ptr_t pMemtable);

  interface_lsmtree_segment_t(const interface_lsmtree_segment_t &) = delete;
  interface_lsmtree_segment_t &
  operator=(const interface_lsmtree_segment_t &) = delete;

  interface_lsmtree_segment_t(interface_lsmtree_segment_t &&) = delete;
  interface_lsmtree_segment_t &
  operator=(interface_lsmtree_segment_t &&) = delete;

  virtual ~interface_lsmtree_segment_t() = default;

  [[nodiscard]] name_t get_name() const;
  [[nodiscard]] path_t get_path() const;

  [[nodiscard]] virtual std::optional<lsmtree::record_t> 
  get_record(const lsmtree::key_t &key) = 0;

  virtual void flush() = 0;

protected:
  memtable_unique_ptr_t m_pMemtable;

private:
  const std::filesystem::path m_path;
};

using segment_shared_ptr_t = std::shared_ptr<interface_lsmtree_segment_t>;

} // namespace structures::lsmtree

#endif // ZKV_INTERFACE_LSMTREE_SEGMENT_H
