# tinykvpp

## Description
This is my second attempt to build a distributed key-value store. The latest version of the first attempt lives in [abandoned](https://github.com/lnikon/tinykvpp/tree/abandoned) and will not be continued. Lots of lessons were learned and I'm going to really try not to produce a mess this time, and come up with something adequate.

## Build

## ToDo
- [ ] Memory leak tracing allocator
- [ ] Shared arena between memtable and skiplist
- [ ] Support tombstones
- [ ] Engine/DB: A fat struct keeping arenas, memtables, sequence number, wal, etc... together
- [ ] Memtable rotation: active and imm memtables