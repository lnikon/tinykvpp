#include <benchmark/benchmark.h>

#include "db/db.h"

#include <chrono>

using mem_key_t = structures::memtable::memtable_t::record_t::key_t;
using mem_value_t = structures::memtable::memtable_t::record_t::value_t;

auto generateRandomString(int length) -> std::string
{
    std::random_device                 rng;
    std::mt19937                       generator(rng());      // Mersenne Twister random number generator
    std::uniform_int_distribution<int> distribution(32, 126); // ASCII printable range

    std::string result;
    for (int i = 0; i < length; ++i)
    {
        char randomChar = static_cast<char>(distribution(generator));
        result += randomChar;
    }

    return result;
}

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
        std::cerr << "unable to open the db" << std::endl;
        return 1;
    }

    const std::vector<std::size_t> recordNumbers{1 << 2, 1 << 4, 1 << 8, 1 << 16};
    for (const auto recordsNumber : recordNumbers)
    {
        std::vector<std::pair<mem_key_t, mem_value_t>> records;
        records.reserve(recordsNumber);
        for (std::size_t idx{0}; idx < recordsNumber; idx++)
        {
            records.emplace_back(generateRandomString(1024), generateRandomString(1024));
        }

        auto start = high_resolution_clock::now();
        for (const auto &record : records)
        {
            db.put(record.first, record.second);
        }
        auto end = high_resolution_clock::now();

        auto ms_int = duration_cast<milliseconds>(end - start);
        std::cout << "Time spent putting " << recordsNumber << " records: " << ms_int << std::endl;
    }

    return 0;
}
