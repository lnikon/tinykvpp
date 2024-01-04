//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>
#include <structures/lsmtree/segments/lsmtree_segment_manager.h>

namespace structures::lsmtree::segment_manager {

lsmtree_segment_manager_t::lsmtree_segment_manager_t(
    const config::sptr_t &config, lsmtree::segment_storage::sptr pStorage)
    : m_config{config}, m_pStorage{pStorage} {
  assert(!m_config->LSMTreeConfig.SegmentsDirectoryName.empty());
}

segment_shared_ptr_t lsmtree_segment_manager_t::get_new_segment(
    const structures::lsmtree::lsmtree_segment_type_t type,
    memtable_unique_ptr_t pMemtable) {
  const auto path{construct_path(get_next_name())};
  return lsmtree_segment_factory(type, path, std::move(pMemtable));
}

segment_shared_ptr_t
lsmtree_segment_manager_t::get_segment(const std::string &name) {
  return m_pStorage->get(name);
}

lsmtree::segment_storage::sptr lsmtree_segment_manager_t::get_segments() {
  return m_pStorage->shared_from_this();
}

std::vector<lsmtree_segment_manager_t::segment_name_t>
lsmtree_segment_manager_t::get_segment_names() const {
  std::vector<segment_name_t> result;
  result.reserve(m_pStorage->size());
  for (const auto &segment : *m_pStorage) {
    result.emplace_back(segment->get_name());
  }
  return result;
}

std::vector<std::filesystem::path>
lsmtree_segment_manager_t::get_segment_paths() const {
  const auto &names = get_segment_names();
  auto paths = std::vector<std::filesystem::path>{};
  paths.reserve(names.size());
  for (const auto &name : names) {
    paths.emplace_back(construct_path(name));
  }
  return paths;
}

// TODO(vahag): Find better naming strategy
std::string lsmtree_segment_manager_t::get_next_name() {
  // TODO(lnikon): Use timestamp instead of a index
  return "segment_" + std::to_string(m_index++);
}

std::filesystem::path
lsmtree_segment_manager_t::construct_path(const std::string &name) const {
  return m_config->get_segments_path() / name;
}

} // namespace structures::lsmtree::segment_manager
