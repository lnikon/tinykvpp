#pragma once

#include <structures/lsmtree/lsmtree.h>

class db_t {
public:
	using lsmtree_t = structures::lsmtree::lsmtree_t;

  db_t(lsmtree_t &lsmTree);

private:
  lsmtree_t &m_lsmTree;
};
