//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_INTERFACE_LSMTREE_SEGMENT_H
#define ZKV_INTERFACE_LSMTREE_SEGMENT_H

#include <structures/lsmtree/lsmtree_types.h>

#include <filesystem>
#include <memory>
#include <string>

namespace structures::lsmtree {

class interface_lsmtree_segment_t {
public:
  explicit interface_lsmtree_segment_t(std::filesystem::path path,
                                       memtable_unique_ptr_t pMemtable);

  interface_lsmtree_segment_t(const interface_lsmtree_segment_t &) = delete;
  interface_lsmtree_segment_t &
  operator=(const interface_lsmtree_segment_t &) = delete;

  interface_lsmtree_segment_t(interface_lsmtree_segment_t &&) = delete;
  interface_lsmtree_segment_t &
  operator=(interface_lsmtree_segment_t &&) = delete;

  virtual ~interface_lsmtree_segment_t() = default;

  [[nodiscard]] std::string get_name() const;
  [[nodiscard]] std::filesystem::path get_path() const;

  virtual void flush() = 0;

protected:
  memtable_unique_ptr_t m_pMemtable;

private:
  const std::filesystem::path m_path;
};

using segment_shared_ptr_t = std::shared_ptr<interface_lsmtree_segment_t>;

} // namespace structures::lsmtree

#endif // ZKV_INTERFACE_LSMTREE_SEGMENT_H
