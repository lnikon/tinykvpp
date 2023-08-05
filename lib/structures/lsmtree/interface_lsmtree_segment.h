//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_INTERFACE_LSMTREE_SEGMENT_H
#define ZKV_INTERFACE_LSMTREE_SEGMENT_H

#include <memory>
#include <string>

namespace structures::lsmtree {

class interface_lsmtree_segment_t {
public:
  explicit interface_lsmtree_segment_t(std::string name);

  virtual void set_content(std::string content);
  [[nodiscard]] std::string get_name() const;
  virtual void flush() = 0;

protected:
  std::string m_content;

private:
  const std::string m_name;
};

using shared_ptr_t = std::shared_ptr<interface_lsmtree_segment_t>;

} // namespace structures::lsmtree

#endif // ZKV_INTERFACE_LSMTREE_SEGMENT_H
