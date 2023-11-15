## tinykvpp

## Dependencies
Make sure that you've installed:

* Conan >= 2.0.7

* CMake >= 3.23

## Content
The following is included:

* conanfile.txt - dependencies for package manager

* CMakeLists.txt - minimal CMake config which creates static libs, tests, drivers, and links against dependencies

## Build 
First, install and build missing dependencies using Conan:

`conan install . --output-folder=build --build=missing --profile=default`

Then generate a build config for conan-release preset.

`cmake --preset conan-release`

## Resources

* [Understanding: Log-Structured Merge Trees](https://otee.dev/2023/04/17/log-structured-merge-tree.html)
