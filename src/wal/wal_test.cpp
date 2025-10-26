#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <ranges>

#include <spdlog/spdlog.h>

#include "wal/common.h"
#include "wal/wal.h"

namespace
{

auto GetTemporaryFilepath() -> fs::path_t
{
    return std::filesystem::temp_directory_path() / "temp.txt";
}

struct rec_t
{
    std::string key;
    std::string value;
};

auto operator<<(std::ostream &stream, const rec_t &record) -> std::ostream &
{
    stream << record.key << ' ' << record.value << '\n';
    return stream;
}

auto operator>>(std::istream &stream, rec_t &record) -> std::istream &
{
    stream >> record.key;
    stream >> record.value;
    return stream;
}
} // namespace

// ----
// ---- Test WAL
// ----

class WALTest : public testing::TestWithParam<wal::log_storage_type_k>
{
  public:
    static std::vector<rec_t> Recs;

    static void SetUpTestSuite()
    {
        spdlog::set_level(spdlog::level::off);
    }

    static void TearDownTestSuite()
    {
        spdlog::set_level(spdlog::level::trace);
    }

    void TearDown() override
    {
        std::filesystem::remove(GetTemporaryFilepath());
    }

    static auto GetWAL() -> std::optional<wal::wal_t<rec_t>>
    {
        return wal::wal_builder_t{}
            .set_file_path(GetTemporaryFilepath())
            .build<rec_t>(WALTest::GetParam());
    }
};

std::vector<rec_t> WALTest::Recs = {
    {.key = "key1", .value = "value1"},
    {.key = "key2", .value = "value2"},
    {.key = "key3", .value = "value3"},
    {.key = "key4", .value = "value4"},
    {.key = "key5", .value = "value5"},
    {.key = "key6", .value = "value6"},
    {.key = "key7", .value = "value7"}
};

TEST_P(WALTest, SuccessfullyCreatesEmptyWAL)
{
    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    EXPECT_EQ(wal->size(), 0);
    EXPECT_TRUE(wal->empty());

    auto res{wal->read(0)};
    EXPECT_FALSE(res.has_value());
}

TEST_P(WALTest, AppendAndCheckSize)
{
    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());
    EXPECT_TRUE(wal->add(Recs[0]));

    EXPECT_EQ(wal->size(), 1);
    EXPECT_FALSE(wal->empty());

    auto res{wal->read(0)};
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res->key, Recs[0].key);
    EXPECT_EQ(res->value, Recs[0].value);
}

TEST_P(WALTest, AppendSameEntryAndCheckSize)
{
    const auto recs = Recs | std::views::take(3);

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }

    const auto walSize = wal->size();
    EXPECT_EQ(walSize, recs.size());
    EXPECT_FALSE(wal->empty());

    for (const auto &[idx, rec] : std::views::enumerate(recs))
    {
        auto res{wal->read(idx)};
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res->key, rec.key);
        EXPECT_EQ(res->value, rec.value);
    }
}

TEST_P(WALTest, AppendAndRestAndCheckSize)
{
    const auto recs = Recs | std::views::take(3);

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }

    EXPECT_EQ(wal->size(), recs.size());
    EXPECT_FALSE(wal->empty());
    EXPECT_TRUE(wal->reset());
    EXPECT_EQ(wal->size(), 0);
    EXPECT_TRUE(wal->empty());
}

TEST_P(WALTest, AppendAndResetLastNAndCheckSize)
{
    const std::size_t        lastN = 3;
    const std::vector<rec_t> recs = Recs;

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }

    const auto walSizeBeforeResetLastN = wal->size();
    EXPECT_EQ(walSizeBeforeResetLastN, recs.size());
    EXPECT_FALSE(wal->empty());

    EXPECT_TRUE(wal->reset_last_n(lastN));

    const auto walSizeAfteresetLastN = wal->size();
    EXPECT_EQ(walSizeAfteresetLastN, walSizeBeforeResetLastN - lastN);
    EXPECT_FALSE(wal->empty());

    for (const auto &[idx, rec] :
         recs | std::views::enumerate | std::views::take(recs.size() - lastN))
    {
        auto res{wal->read(idx)};
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res->key, rec.key);
        EXPECT_EQ(res->value, rec.value);
    }
}

TEST_P(WALTest, ResetLastNOnEmptyWAL)
{
    auto wal = WALTest::GetWAL();

    EXPECT_FALSE(wal->reset_last_n(std::numeric_limits<std::size_t>::min()));
    EXPECT_FALSE(wal->reset_last_n(std::numeric_limits<std::size_t>::max() / 2));
    EXPECT_FALSE(wal->reset_last_n(std::numeric_limits<std::size_t>::max()));
}

TEST_P(WALTest, ResetAndAdd)
{
    const std::vector<rec_t> recs = Recs;

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }
    EXPECT_EQ(wal->size(), recs.size());

    EXPECT_TRUE(wal->reset());
    EXPECT_EQ(wal->size(), 0);

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }
    EXPECT_EQ(wal->size(), recs.size());
}

TEST_P(WALTest, ResetLasnNAndAdd)
{
    const std::size_t        lastN = 3;
    const std::vector<rec_t> recs = Recs;

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }

    const auto walSizeBeforeResetLastN = wal->size();
    EXPECT_EQ(walSizeBeforeResetLastN, recs.size());
    EXPECT_FALSE(wal->empty());

    EXPECT_TRUE(wal->reset_last_n(lastN));

    const auto walSizeAfteresetLastN = wal->size();
    EXPECT_EQ(walSizeAfteresetLastN, walSizeBeforeResetLastN - lastN);
    EXPECT_FALSE(wal->empty());

    EXPECT_TRUE(wal->add(recs[4]));
    EXPECT_EQ(wal->size(), walSizeBeforeResetLastN - lastN + 1);

    for (const auto &[idx, rec] :
         recs | std::views::enumerate | std::views::take(recs.size() - lastN + 1))
    {
        auto res{wal->read(idx)};
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res->key, rec.key);
        EXPECT_EQ(res->value, rec.value);
    }
}

TEST_P(WALTest, GetRecordsOnEmptyWAL)
{
    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());
    EXPECT_EQ(wal->records().size(), 0);
}

TEST_P(WALTest, AppendAndGetRecords)
{
    const auto recs = Recs | std::views::take(3);

    auto wal = WALTest::GetWAL();

    EXPECT_TRUE(wal.has_value());

    for (const auto &rec : recs)
    {
        EXPECT_TRUE(wal->add(rec));
    }

    EXPECT_EQ(wal->size(), recs.size());
    EXPECT_FALSE(wal->empty());

    std::vector<rec_t> walRecs = wal->records();
    EXPECT_EQ(recs.size(), walRecs.size());

    for (const auto &[idx, rec] : std::views::enumerate(recs))
    {
        EXPECT_EQ(rec.key, walRecs[idx].key);
        EXPECT_EQ(rec.value, walRecs[idx].value);
    }
}

TEST_P(WALTest, ReadOutOfBounds)
{
    auto wal = WALTest::GetWAL();

    ASSERT_TRUE(wal.has_value());
    EXPECT_FALSE(wal->read(-1).has_value());
    EXPECT_FALSE(wal->read(1000).has_value());
}

// ----
// ---- Test WAL recovery
// ----

TEST_P(WALTest, CreateAndRecoverInMemoryWAL)
{
    if (WALTest::GetParam() == wal::log_storage_type_k::file_based_persistent_k)
    {
        GTEST_SKIP();
    }

    // Create initial WAL
    auto initialWal = wal::wal_builder_t{}
                          .set_file_path(GetTemporaryFilepath())
                          .build<rec_t>(wal::log_storage_type_k::in_memory_k);

    // Fill it will mock data
    for (const auto &rec : WALTest::Recs)
    {
        EXPECT_TRUE(initialWal->add(rec));
    }
    EXPECT_EQ(initialWal->size(), WALTest::Recs.size());

    // Recover the WAL
    auto recoveredWal = wal::wal_builder_t{}
                            .set_file_path(GetTemporaryFilepath())
                            .build<rec_t>(wal::log_storage_type_k::in_memory_k);

    // Main assertions
    EXPECT_NE(initialWal->size(), recoveredWal->size());
    EXPECT_EQ(recoveredWal->size(), 0);
    EXPECT_TRUE(recoveredWal->empty());
}

TEST_P(WALTest, CreateAndRecoverFileBasedWAL)
{
    if (WALTest::GetParam() == wal::log_storage_type_k::in_memory_k)
    {
        GTEST_SKIP_("Skipping WAL recovery testing for in-memory storage");
    }

    // Create initial WAL
    auto initialWal = wal::wal_builder_t{}
                          .set_file_path(GetTemporaryFilepath())
                          .build<rec_t>(wal::log_storage_type_k::file_based_persistent_k);

    // Fill it will mock data
    for (const auto &rec : WALTest::Recs)
    {
        EXPECT_TRUE(initialWal->add(rec));
    }
    EXPECT_EQ(initialWal->size(), WALTest::Recs.size());
    auto initialRecords = initialWal->records();

    // Recover the WAL
    auto recoveredWal = wal::wal_builder_t{}
                            .set_file_path(GetTemporaryFilepath())
                            .build<rec_t>(wal::log_storage_type_k::file_based_persistent_k);
    auto recoveredRecords = initialWal->records();

    // Main assertions
    EXPECT_EQ(initialWal->size(), recoveredWal->size());

    for (std::size_t idx{0}; idx < initialWal->size(); idx++)
    {
        EXPECT_EQ(initialWal->read(idx)->key, recoveredWal->read(idx)->key);
        EXPECT_EQ(initialWal->read(idx)->value, recoveredWal->read(idx)->value);

        EXPECT_EQ(initialRecords[idx].key, recoveredRecords[idx].key);
        EXPECT_EQ(initialRecords[idx].value, recoveredRecords[idx].value);
    }
}

// ----
// ---- Test WAL builder
// ----
TEST_P(WALTest, InMemoryDoesNotCreateFile)
{
    if (WALTest::GetParam() == wal::log_storage_type_k::file_based_persistent_k)
    {
        GTEST_SKIP();
    }

    auto wal = wal::wal_builder_t{}
                   .set_file_path(GetTemporaryFilepath())
                   .build<rec_t>(wal::log_storage_type_k::in_memory_k);

    EXPECT_TRUE(wal.has_value());
    EXPECT_FALSE(std::filesystem::exists(GetTemporaryFilepath()));
}

TEST_P(WALTest, PersistentCreateFile)
{
    if (WALTest::GetParam() == wal::log_storage_type_k::in_memory_k)
    {
        GTEST_SKIP();
    }

    auto wal = wal::wal_builder_t{}
                   .set_file_path(GetTemporaryFilepath())
                   .build<rec_t>(wal::log_storage_type_k::file_based_persistent_k);

    EXPECT_TRUE(wal.has_value());
    EXPECT_TRUE(std::filesystem::exists(GetTemporaryFilepath()));
}

TEST_P(WALTest, FailedToOpenFileWithNoReadWrite)
{
    if (WALTest::GetParam() == wal::log_storage_type_k::in_memory_k)
    {
        GTEST_SKIP();
    }

    std::ofstream tempFile(GetTemporaryFilepath());
    std::filesystem::permissions(
        GetTemporaryFilepath(),
        std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
        std::filesystem::perm_options::remove
    );

    const auto recs = Recs;

    auto wal = wal::wal_builder_t{}
                   .set_file_path(GetTemporaryFilepath())
                   .build<rec_t>(wal::log_storage_type_k::file_based_persistent_k);
    EXPECT_FALSE(wal.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    LogStorageTypes,
    WALTest,
    testing::Values(
        wal::log_storage_type_k::in_memory_k, wal::log_storage_type_k::file_based_persistent_k
    )
);
