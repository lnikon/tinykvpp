#include <celero/Benchmark.h>
#include <celero/Celero.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <cstddef>

#include "config/config.h"
#include "db/db.h"
#include "common.h"

CELERO_MAIN

class DBPutGet_CeleroFixture : public celero::TestFixture
{
  public:
    DBPutGet_CeleroFixture()
    {
        spdlog::set_level(spdlog::level::off);

        if (m_pDb != nullptr)
        {
            return;
        }

        auto pConfig = config::make_shared();
        pConfig->LSMTreeConfig.DiskFlushThresholdSize = 1024 * 1024 * 1024;
        pConfig->LSMTreeConfig.LevelZeroCompactionThreshold = 1024 * 1024 * 1024;
        pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold = 1024 * 1024 * 1024;
        m_pDb = new db::db_t(pConfig);
        if (!m_pDb->open())
        {
            spdlog::critical("Unable to open the DB");
            exit(EXIT_FAILURE);
        }
    }

    auto getExperimentValues() const -> std::vector<std::shared_ptr<celero::TestFixture::ExperimentValue>> override
    {
        constexpr const std::size_t                                        totalNumberOfTests = 12;
        std::vector<std::shared_ptr<celero::TestFixture::ExperimentValue>> problemSpaceValues;

        problemSpaceValues.reserve(totalNumberOfTests);
        for (std::size_t i = 0; i < totalNumberOfTests; i++)
        {
            problemSpaceValues.push_back(std::make_shared<celero::TestFixture::ExperimentValue>(
                int64_t(pow(2, static_cast<double>(i + 1))),
                int64_t(pow(2, static_cast<double>(totalNumberOfTests - i)))));
        }

        return problemSpaceValues;
    }

    void setUp(const celero::TestFixture::ExperimentValue *const experimentValue) override
    {
        m_records.reserve(1024);
        keySize = experimentValue->Value;
        valueSize = experimentValue->Value;
    }

    void randomize()
    {
        for (std::size_t idx{0}; idx < m_records.capacity(); idx++)
        {
            m_records.emplace_back(bench::generateRandomString(keySize), bench::generateRandomString(valueSize));
        }
    }

    void clear()
    {
        m_records.clear();
    }

    void tearDown() override
    {
    }

    static db::db_t                                             *m_pDb;
    std::size_t                                                  keySize = 1024;
    std::size_t                                                  valueSize = 1024;
    std::vector<std::pair<bench::mem_key_t, bench::mem_value_t>> m_records;
};

db::db_t *DBPutGet_CeleroFixture::m_pDb = nullptr;

BASELINE_F(DemoTransform, ForLoop, DBPutGet_CeleroFixture, 10, 1000)
{
    this->randomize();
    for (const auto &record : this->m_records)
    {
        m_pDb->put(record.first, record.second);
    }
    this->clear();
}
