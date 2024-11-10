#include <benchmark/benchmark.h>

#include "db/db.h"

#include <spdlog/spdlog.h>

using mem_key_t = structures::memtable::memtable_t::record_t::key_t;
using mem_value_t = structures::memtable::memtable_t::record_t::value_t;

auto generateRandomString(int length) -> std::string
{
    std::mt19937                       generator(time(nullptr)); // Mersenne Twister random number generator
    std::uniform_int_distribution<int> distribution(32, 126);    // ASCII printable range

    std::string result;
    for (int i = 0; i < length; ++i)
    {
        char randomChar = static_cast<char>(distribution(generator));
        result += randomChar;
    }

    return result;
}

class DatabaseFixture : public benchmark::Fixture
{
  public:
    void SetUp(::benchmark::State &state) override
    {
        (void)state;
        spdlog::set_level(spdlog::level::off);
        if (m_pDb == nullptr)
        {
            auto pConfig = config::make_shared();
            pConfig->LSMTreeConfig.DiskFlushThresholdSize = 1024 * 1024 * 1024;
            pConfig->LSMTreeConfig.LevelZeroCompactionThreshold = 1024 * 1024 * 1024;
            pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold = 1024 * 1024 * 1024;
            m_pDb = new db::db_t(pConfig);
        }
    }

    void TearDown(::benchmark::State &state) override
    {
        (void)state;
    }

    static db::db_t *m_pDb;
};

db::db_t *DatabaseFixture::m_pDb = nullptr;

// Defines and registers `PutTest` using the class `DatabaseFixture`.
BENCHMARK_DEFINE_F(DatabaseFixture, PutTest)(benchmark::State &st)
{
    std::vector<std::pair<mem_key_t, mem_value_t>> records;
    for (auto _ : st)
    {
        st.PauseTiming();
        const std::size_t recordNum{1024};
        records.reserve(recordNum);
        for (std::size_t i = 0; i < recordNum; i++)
        {
            records.emplace_back(generateRandomString(st.range(0)), generateRandomString(st.range(0)));
        }
        st.ResumeTiming();

        for (const auto &record : records)
        {
            DatabaseFixture::m_pDb->put(record.first, record.second);
        }
    }
}
BENCHMARK_REGISTER_F(DatabaseFixture, PutTest)->Arg(8)->Arg(64)->Arg(512)->Arg(4 << 10)->Arg(8 << 10)->Arg(16 << 10);

BENCHMARK_MAIN();
