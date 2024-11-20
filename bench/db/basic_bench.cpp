#include <spdlog/spdlog.h>

#include "common.h"
#include "db/db.h"
#include "config/config.h"

auto main() -> int
{
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::milliseconds;

    auto pConfig = config::make_shared();
    pConfig->LSMTreeConfig.DiskFlushThresholdSize = 1024 * 1024 * 1024;
    pConfig->LSMTreeConfig.LevelZeroCompactionThreshold = 1024 * 1024 * 1024;
    pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold = 1024 * 1024 * 1024;
    db::db_t db(pConfig);
    if (!db.open())
    {
        spdlog::critical("Unable to open DB");
        return 1;
    }

    const std::vector<std::size_t> recordNumbers{1 << 2, 1 << 4, 1 << 8, 1 << 16};
    for (const auto recordsNumber : recordNumbers)
    {
        std::vector<std::pair<bench::mem_key_t, bench::mem_value_t>> records;
        records.reserve(recordsNumber);
        for (std::size_t idx{0}; idx < recordsNumber; idx++)
        {
            records.emplace_back(bench::generateRandomString(1024), bench::generateRandomString(1024));
        }

        auto start = high_resolution_clock::now();
        for (const auto &record : records)
        {
            db.put(record.first, record.second);
        }
        auto end = high_resolution_clock::now();

        auto ms_int = duration_cast<milliseconds>(end - start);
        spdlog::info("Time spent putting {} records {}", recordsNumber, ms_int.count());
    }

    return 0;
}
