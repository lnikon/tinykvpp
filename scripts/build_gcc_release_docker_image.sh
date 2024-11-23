#!/bin/sh

docker build --build-arg TARGET=gcc:latest --build-arg COMPILER=gcc --build-arg BUILD_TYPE=release . --file Dockerfile --tag tinykvpp-gcc
