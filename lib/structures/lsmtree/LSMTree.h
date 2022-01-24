//
// Created by nikon on 1/21/22.
//

#ifndef CPP_PROJECT_TEMPLATE_LSMTREE_H
#define CPP_PROJECT_TEMPLATE_LSMTREE_H

#include "structures/memtable/MemTable.h"

namespace structures {
    namespace lsmtree {
        using MemTable = structure::memtable::MemTable;
        using Record = structure::memtable::MemTable::Record;
        using Key = MemTable::Record::Key;
        using Value = MemTable::Record::Value;

        struct LSMTreeConfig
        {
            /*
             * Determines the size (in Mb) of the table after which it should be flushed onto the disk.
             */
            const std::size_t DefaultDiskFlushThresholdSize {8 * 1024 * 1024}; // 8 Megabyte
            std::size_t DiskFlushThresholdSize{DefaultDiskFlushThresholdSize};
        };

        class LSMTree {
        public:
            LSMTree(const LSMTreeConfig& config)
                : m_config(config)
            {

            }

            LSMTree() = default;
            LSMTree(const LSMTree&) = delete;
            LSMTree& operator=(const LSMTree&) = delete;
            LSMTree(LSMTree&&) = delete;
            LSMTree& operator=(LSMTree&&) = delete;

            // TODO: Both Key and Value should support size!
            void Insert(const Key& key, const Value& value)
            {
                std::lock_guard lg(m_mutex);
                // 1. Construct record
                // 2. Sum up records and table size, if greater then threshold, flush on disk

                if (key.size() + value.size() + m_table.size() >= )
                m_table.emplace(Record{key, value});
            }

        private:
            std::mutex m_mutex;
            LSMTreeConfig m_config;
            MemTable m_table;
            std::size_t m_size;
            // TODO: Keep BloomFilter(BF) for reads. First check BF, if it says no, then abort searching. Otherwise perform search.
        };
    } // lsmtree
} // structures

#endif //CPP_PROJECT_TEMPLATE_LSMTREE_H
