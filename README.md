# tinykvpp

## Build
tinykvpp uses Conan2 as it's package manager. CMake is the preferred tool for the project generation.

### Dependencies
Currently, tinykvpp depends on following C++ libraries:
* catch2/3.5.3
* spdlog/1.13.0
* fmt/10.2.1

### Build 
Innstall and build missing dependencies using Conan2. Following command will use a `default` profile. If you are considering to develop and debug the project, consider using `debug` profile. Note that Conan2 doesn't ship with `debug` profile by default, so you may need to write your own. 

`conan install . --output-folder=build --build=missing --profile=default`

Then generate a build config for conan-release preset.

`cmake --preset conan-release`

and then build the project:

`cmake --build ./build`

## Resources

* [Understanding: Log-Structured Merge Trees](https://otee.dev/2023/04/17/log-structured-merge-tree.html)
