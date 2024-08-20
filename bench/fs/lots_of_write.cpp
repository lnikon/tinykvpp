//
// Created by nikon on 8/17/24.
//

#include <fstream>
#include <iostream>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>

void fstream_test()
{
    const std::string filename("test_stream_3.txt");
    std::fstream fs(filename, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::ate);
    if (!fs.is_open())
    {
        std::cerr << "unable to open" << filename << '\n';
        exit(EXIT_FAILURE);
    }

    size_t count{9 * 1024};
    std::string payload("aaa");
    while (count-- != 0)
    {
        fs << payload;
    }
}

void posix_write_test()
{
    const std::string filename("test_stream_2.txt");
    int fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        std::cerr << "Unable to open " << filename << '\n';
    }

    std::string payload("bbbbb");
    size_t count{9 * 1024};
    while (count-- != 0)
    {
        write(fd, payload.c_str(), payload.size());
    }
}

int main()
{
    //    fstream_test();
    posix_write_test();
    return 0;
}
