#include <benchmark/benchmark.h>

#include <fstream>
#include <iostream>
#include <filesystem>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <cstdio>
#include <unistd.h>

static void BM_BenchmarkFstreamWrite(benchmark::State &state)
{
    const std::string filename("test_stream.txt");
    std::fstream      fs(filename, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::ate);
    if (!fs.is_open())
    {
        std::cerr << "unable to open" << filename << '\n';
        exit(EXIT_FAILURE);
    }

    std::string payload("aaaaa");
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(fs.write(payload.c_str(), payload.size()));
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFstreamWrite);

static void BM_BenchmarkFstreamWriteWithFlush(benchmark::State &state)
{
    const std::string filename("test_stream.txt");
    std::fstream      fs(filename, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::ate);
    if (!fs.is_open())
    {
        std::cerr << "unable to open" << filename << '\n';
        exit(EXIT_FAILURE);
    }

    std::string payload("aaaaa");
    for (auto _ : state)
    {
        fs.write(payload.c_str(), payload.size());
        fs.flush();
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFstreamWriteWithFlush);

static void BM_BenchmarkFstreamWriteWithSync(benchmark::State &state)
{
    const std::string filename("test_stream.txt");
    std::fstream      fs(filename, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::ate);
    if (!fs.is_open())
    {
        std::cerr << "unable to open" << filename << '\n';
        exit(EXIT_FAILURE);
    }

    std::string payload("aaaaa");
    for (auto _ : state)
    {
        fs.write(payload.c_str(), payload.size());
        fs.sync();
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFstreamWriteWithSync);

static void BM_BenchmarkFstreamWriteNoBuffering(benchmark::State &state)
{
    const std::string filename("test_stream.txt");
    std::fstream      fs;
    fs.rdbuf()->pubsetbuf(nullptr, 0);

    fs.open(filename, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::ate);
    if (!fs.is_open())
    {
        std::cerr << "unable to open" << filename << '\n';
        exit(EXIT_FAILURE);
    }

    std::string payload("aaaaa");
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(fs.write(payload.c_str(), payload.size()));
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFstreamWriteNoBuffering);

static void BM_BenchmarkPosixWrite(benchmark::State &state)
{
    const std::string filename("test_stream_2.txt");
    int               fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::string payload("bbbbb");
    for (auto _ : state)
    {
        write(fd, payload.c_str(), payload.size());
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkPosixWrite);

static void BM_BenchmarkPosixWriteODirect(benchmark::State &state)
{
    const std::string filename("test_stream_2.txt");
    int               fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_DIRECT, 0644);
    if (fd == -1)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::string payload("bbbbb");
    for (auto _ : state)
    {
        write(fd, payload.c_str(), payload.size());
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkPosixWriteODirect);

static void BM_BenchmarkPosixScatterWrite(benchmark::State &state)
{
    const std::string filename("test_stream_2.txt");
    int               fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::vector<iovec> iov;
    std::string        payload("bbbbb");
    for (auto _ : state)
    {
        iov.emplace_back(iovec{.iov_base = payload.data(), .iov_len = payload.size()});
    }
    writev(fd, iov.data(), iov.size());

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkPosixScatterWrite);

static void BM_BenchmarkPosixScatterWriteWithODirect(benchmark::State &state)
{
    const std::string filename("test_stream_2.txt");
    int               fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_DIRECT, 0644);
    if (fd == -1)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::vector<iovec> iov;
    std::string        payload("bbbbb");
    for (auto _ : state)
    {
        iov.emplace_back(iovec{.iov_base = payload.data(), .iov_len = payload.size()});
    }
    writev(fd, iov.data(), iov.size());

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkPosixScatterWriteWithODirect);

static void BM_BenchmarkFWrite(benchmark::State &state)
{
    const std::string filename("test_stream_3.txt");
    FILE             *file = fopen(filename.c_str(), "w");
    if (file == nullptr)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::string payload("bbbbb");
    for (auto _ : state)
    {
        fwrite(payload.data(), sizeof(char), payload.size(), file);
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFWrite);

static void BM_BenchmarkFWriteNoBuffering(benchmark::State &state)
{
    const std::string filename("test_stream_3.txt");
    FILE             *file = fopen(filename.c_str(), "w");
    if (file == nullptr)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    // Enable/disable buffering
    setbuf(file, nullptr);

    std::string payload("bbbbb");
    for (auto _ : state)
    {
        fwrite(payload.data(), sizeof(char), payload.size(), file);
    }

    std::filesystem::remove(filename);
}
BENCHMARK(BM_BenchmarkFWriteNoBuffering);

BENCHMARK_MAIN();
