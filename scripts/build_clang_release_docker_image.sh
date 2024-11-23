#!/bin/sh

docker build --build-arg TARGET=silkeh/clang:latest --build-arg COMPILER=clang --build-arg BUILD_TYPE=release . --file Dockerfile --tag tinykvpp-clang
