#include <spdlog/common.h>

#include <celero/Benchmark.h>
#include <celero/Celero.h>

#include <memory>

#ifndef _WIN32
#include <cmath>
#include <cstdlib>
#endif

#include "db/db.h"

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

///
/// This is the main(int argc, char** argv) for the entire celero program.
/// You can write your own, or use this macro to insert the standard one into the project.
///
CELERO_MAIN

constexpr int Multiple = 2112;

///
/// \class	DemoTransformFixture
///	\autho	John Farrier
///
///	\brief	A Celero Test Fixture for array transforms.
///
/// This test fixture will build a experiment of powers of two.  When executed,
/// the selected experiment is used to create an array of the given size, then
/// push random integers into the array.  Each transform function will then
/// modify the randomly generated array for timing.
///
///	This demo highlights how to use the ExperimentValues to build automatic
/// test cases which can scale.  These test cases should ideally be written
/// to a file when executed.  The resulting CSV file can be easily plotted
/// in an application such as Microsoft Excel to show how various
/// tests performed as their experiment scaled.
///
/// \code
/// celeroDemo outfile.csv
/// \endcode
///
class DemoTransformFixture : public celero::TestFixture
{
  public:
    DemoTransformFixture()
    {
        spdlog::set_level(spdlog::level::off);

        if (m_pDb)
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
            std::cerr << "unable to open the db" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    auto getExperimentValues() const -> std::vector<std::shared_ptr<celero::TestFixture::ExperimentValue>> override
    {
        std::vector<std::shared_ptr<celero::TestFixture::ExperimentValue>> problemSpaceValues;

        // We will run some total number of sets of tests all together.
        // Each one growing by a power of 2.
        const int totalNumberOfTests = 12;

        for (int i = 0; i < totalNumberOfTests; i++)
        {
            // ExperimentValues is part of the base class and allows us to specify
            // some values to control various test runs to end up building a nice graph.
            // We make the number of iterations decrease as the size of our problem space increases
            // to demonstrate how to adjust the number of iterations per sample based on the
            // problem space size.
            problemSpaceValues.push_back(std::make_shared<celero::TestFixture::ExperimentValue>(
                int64_t(pow(2, i + 1)), int64_t(pow(2, totalNumberOfTests - i))));
        }

        return problemSpaceValues;
    }

    /// Before each run, build a vector of random integers.
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
            m_records.emplace_back(generateRandomString(keySize), generateRandomString(valueSize));
        }
    }

    void clear()
    {
        m_records.clear();
    }

    /// After each run, clear the vector of random integers.
    void tearDown() override
    {
    }

    static db::db_t                               *m_pDb;
    std::size_t                                    keySize = 1024;
    std::size_t                                    valueSize = 1024;
    std::vector<std::pair<mem_key_t, mem_value_t>> m_records;
};

const std::size_t arraySize = 1024;

db::db_t *DemoTransformFixture::m_pDb = nullptr;

// For a baseline, I'll chose Bubble Sort.
BASELINE_F(DemoTransform, ForLoop, DemoTransformFixture, 10, 1000)
{
    this->randomize();
    for (const auto &record : this->m_records)
    {
        m_pDb->put(record.first, record.second);
    }
    this->clear();
}

// BASELINE_FIXED_F(DemoTransform, FixedTime, DemoTransformFixture, 1, 100)
// {
// }
//
// BENCHMARK_F(DemoTransform, StdTransform, DemoTransformFixture, 30, 10000)
// {
//     this->randomize();
//     for (const auto &record : this->m_records)
//     {
//         m_pDb->put(record.first, record.second);
//     }
//     this->clear();
// }
//
// BENCHMARK_F(DemoTransform, StdTransformLambda, DemoTransformFixture, 30, 10000)
// {
// 	std::transform(this->arrayIn.begin(), this->arrayIn.end(), this->arrayOut.begin(), [](int in) -> int { return in *
// Multiple; });
// }
//
// BENCHMARK_F(DemoTransform, SelfForLoop, DemoTransformFixture, 30, 10000)
// {
// 	for(int i = 0; i < this->arraySize; i++)
// 	{
// 		this->arrayIn[i] *= Multiple;
// 	}
// }
//
// BENCHMARK_F(DemoTransform, SelfStdTransform, DemoTransformFixture, 30, 10000)
// {
// 	std::transform(this->arrayIn.begin(), this->arrayIn.end(), this->arrayIn.begin(),
// std::bind1st(std::multiplies<int>(), Multiple));
// }
//
// BENCHMARK_F(DemoTransform, SelfStdTransformLambda, DemoTransformFixture, 30, 10000)
// {
// 	std::transform(this->arrayIn.begin(), this->arrayIn.end(), this->arrayIn.begin(), [](int in) -> int { return in *
// Multiple; });
// }
