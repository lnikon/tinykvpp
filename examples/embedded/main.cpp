#include "memtable.h"
#include <config/config.h>
#include <db/db.h>

#include <iostream>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <absl/debugging/stacktrace.h>

using mem_key_t = structures::memtable::memtable_t::record_t::key_t;
using mem_value_t = structures::memtable::memtable_t::record_t::value_t;

auto main() -> int
{
    spdlog::set_level(spdlog::level::info);

    auto pConfig = config::make_shared();
    pConfig->LSMTreeConfig.DiskFlushThresholdSize = 1024;
    pConfig->LSMTreeConfig.LevelZeroCompactionThreshold = 1024;
    pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold = 1024;
    db::db_t db(pConfig);
    if (!db.open())
    {
        spdlog::error("Unable to open the database");
        return 1;
    }

    for (int i = 0; i < 128; i++)
    {
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version1"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version2"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version3"});
        db.put(mem_key_t{"cccccc"}, mem_value_t{"aaaa"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version4"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version5"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version6"});
        db.put(mem_key_t{"cccccc"}, mem_value_t{"bbbb"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version7"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version8"});
        db.put(mem_key_t{"ddddd"}, mem_value_t{"version1"});
        db.put(mem_key_t{"cccccc"}, mem_value_t{"dddd"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version9"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version10"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version11"});
        db.put(mem_key_t{"cccccc1"}, mem_value_t{"aaaa1"});
        db.put(mem_key_t{"aaaaaa"}, mem_value_t{"version12"});
        db.put(mem_key_t{"aaaaaa2"}, mem_value_t{"version13"});
        db.put(mem_key_t{"ddddd1"}, mem_value_t{"version13"});
    }

    // Debugging trick:
    // Wait for the flushing thread to flush all memtables into the segments.
    // This will trick the db to search the segments for the records.
    // std::this_thread::sleep_for(std::chrono::seconds(5));

    if (auto recordOpt{db.get(mem_key_t{"aaaaaa"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"cccccc"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"ddddd"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"cccccc1"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"aaaaaa2"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"aaaaaa4"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"ddddd2"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"cccccc5"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(mem_key_t{"ddddd1"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    return 0;
}
