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
            const std::size_t DefaultDiskFlushThresholdSize{8 * 1024 * 1024}; // 8 Megabyte
            std::size_t DiskFlushThresholdSize{DefaultDiskFlushThresholdSize};

            /*
             * Determines number of segments after which compaction process should start.
             */
            const std::size_t DefaultCompactionSegmentCount{16};
            std::size_t CompactionSegmentCount{DefaultCompactionSegmentCount};
        };

        class LSMTree {
        public:
            // TODO: Make LSMTreeConfig configurable via CLI
            LSMTree(const LSMTreeConfig& config)
                : m_config(config)
            { }

            LSMTree() = default;
            LSMTree(const LSMTree&) = delete;
            LSMTree& operator=(const LSMTree&) = delete;
            LSMTree(LSMTree&&) = delete;
            LSMTree& operator=(LSMTree&&) = delete;

            void Insert(const Key& key, const Value& value)
            {
                std::lock_guard lg(m_mutex);
                if (key.Size() + value.Size() + m_table.Size() >= m_config.DiskFlushThresholdSize)
                {
                    // TODO: For now, lock whole table, dump it into on-disk segment, and replace the table with new one.
                    // TODO: For the future, keep LSMTree readable while dumping.
                }

                m_table.emplace(Record{key, value});
            }

        private:
            std::mutex m_mutex;
            LSMTreeConfig m_config;
            MemTable m_table;
            std::size_t m_size;
            // TODO: Keep BloomFilter(BF) for reads. First check BF, if it says no, then abort searching. Otherwise perform search.
            // TODO: Keep in-memory indices for segments.
        };

        // TODO: This is a scratch code to represent on-disk segments, compaction logic and related stuff.
        template <typename T>
        struct GenericWriter
        {
            // TODO: Maybe change @path with something general?
            // TODO: Maybe use std::filesystem::path instead of std::string?
            void Write(const T& value, const std::string& path)
            {

            }
        };

        template <typename T>
        struct GenericReader
        {
            void Read(T& result)
            {

            }
        };

        struct LSMTreeSegment
        {
            void Read()
            { }

            void Write()
            { }
        };

        struct LSMTreeWriter: GenericWriter<LSMTree>
        {

        };

        struct LSMTreeReader: GenericReader<LSMTreeReader>
        {

        };

        struct LSMTreeSegmentWriter: GenericWriter<LSMTreeSegmentWriter>
        {

        };

        struct LSMTreeSegmentReader: GenericWriter<LSMTreeSegmentReader>
        {

        };
    } // lsmtree
} // structures

#endif //CPP_PROJECT_TEMPLATE_LSMTREE_H
