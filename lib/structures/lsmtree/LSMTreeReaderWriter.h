//
// Created by nikon on 1/26/22.
//

#ifndef ZKV_LSMTREEREADERWRITER_H
#define ZKV_LSMTREEREADERWRITER_H

#include <structures/lsmtree/LSMTree.h>

namespace structures::lsmtree
{

template <typename T>
struct GenericReader
{
    void Read(T& result)
    { }
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

}

#endif //ZKV_LSMTREEREADERWRITER_H
