#include <benchmark/benchmark.h>

#include <spdlog/spdlog.h>

#include "common.h"
#include "db/db.h"
#include "config/config.h"

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
    std::vector<std::pair<bench::mem_key_t, bench::mem_value_t>> records;
    for (auto _ : st)
    {
        const std::size_t recordNum{1024};

        st.PauseTiming();
        records.reserve(recordNum);
        for (std::size_t i = 0; i < recordNum; i++)
        {
            records.emplace_back(bench::generateRandomString(st.range(0)),
                                 bench::generateRandomString(st.range(0)));
        }
        st.ResumeTiming();

        for (const auto &record : records)
        {
            DatabaseFixture::m_pDb->put(record.first, record.second);
        }
    }
}
BENCHMARK_REGISTER_F(DatabaseFixture, PutTest)
    ->Arg(8)
    ->Arg(64)
    ->Arg(512)
    ->Arg(4 << 10)
    ->Arg(8 << 10)
    ->Arg(16 << 10);

BENCHMARK_MAIN();
