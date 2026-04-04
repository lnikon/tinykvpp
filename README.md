# frankie (tinykvpp)

## Description
This is my second attempt to build a distributed key-value store. The latest version of the first attempt lives in [abandoned](https://github.com/lnikon/tinykvpp/tree/abandoned) and will not be continued. Lots of lessons were learned and I'm going to really try not to produce a mess this time, and come up with something adequate.

This is not just a re-write, but an architectural re-imagining of the entire project. No zoo of threads, abstractions on top of abstractions, 'I'll fix it later' bugs.

The idea is to have a future-proof, well-covered, performant storage engine. A main execution loop based on an explicit state machine. Networking layer based on modern kernel interfaces (such as [io_uring](https://www.man7.org/linux/man-pages/man7/io_uring.7.html)). Handcrafted modules for performance-critical parts.

This project serves as an educational playground for me, and producing a production-grade system is not at the top of my priorities (at least now), although as much attention as possible is allocated to error handling, producing clean and readable code, and architecture battle-tested by the industry.

## Build

frankie depends on several third-party packages. For their integration into the build system it was chosen to use [Conan2](https://conan.io/). Otherwise, the build system is fairly typical - CMake + generator of your choice.

[conan2/profiles](./conan2/profiles) directory supplies different profiles (debug, release, sanitizers) for various compilers (GCC, Clang).

Here is an example of how to perform a `debug` build for `GCC`. A `build` folder in current directory will be created, which will contain `Debug` subdirectory. Later, the same `build` directory can be used for `release` builds, with a `Release` subdirectory generated accordingly.

```sh
conan install . --output-folder=. --build=missing --profile=./conan2/profiles/gcc-debug

cmake --preset conan-debug

cmake --build ./build/Debug
```

As a result Conan2 will download and build missing packages. Then, CMake will generate and build the project.

Now, let's perform a `release` build.


```sh
conan install . --output-folder=. --build=missing --profile=./conan2/profiles/gcc-release

cmake --preset conan-release

cmake --build ./build/Release
```

At the moment, there is no entry point such as `int main()` in the project. Instead, different modules ([core](./src/core), [engine](./src/engine), [storage](./src/storage)) are covered with unit tests and benchmarks, to make sure that all building blocks are in place and valid when the integration phase of the project into a proper server begins.

Built tests are located in the build directory `build/Debug` or `build/Release` or the directory under build corresponding to the profile of your choice.
