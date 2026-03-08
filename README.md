# tinykvpp

## Description
This is my second attempt to build a distributed key-value store. The latest version of the first attempt lives in [abandoned](https://github.com/lnikon/tinykvpp/tree/abandoned) and will not be continued. Lots of lessons were learned and I'm going to really try not to produce a mess this time, and come up with something adequate.

## Build

## ToDo
- [ ] Fast rng without kernel that is goood enough
- [ ] skiplist to support insert(node), opaque to allocator
- [ ] Memory leak tracing allocator
- [ ] SIMD comparator ordering different from memcmp
- [ ] Shared arena between memtable and skiplist