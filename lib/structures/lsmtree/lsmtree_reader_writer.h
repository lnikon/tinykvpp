//
// Created by nikon on 1/26/22.
//

#ifndef ZKV_LSMTREEREADERWRITER_H
#define ZKV_LSMTREEREADERWRITER_H

#include "lsmtree.h"

namespace structures::lsmtree {

// TOD(vahag): WTF is this file?

template <typename T> struct generic_reader_t {
  void read(T &result) {}
};

template <typename T> struct generic_writer_t {
  void write(T &result) {}
};

struct lsmtree_segment_t {
  void read() {}
  void write() {}
};

struct lsmtree_writer_t : generic_writer_t<lsmtree_t> {};

struct lsmtree_reader_t : generic_reader_t<lsmtree_reader_t> {};

struct lsmtree_segment_writer_t : generic_writer_t<lsmtree_segment_writer_t> {};

struct lsmtree_segment_reader_t : generic_writer_t<lsmtree_segment_reader_t> {};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREEREADERWRITER_H
