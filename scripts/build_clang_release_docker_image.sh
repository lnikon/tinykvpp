#!/bin/sh

docker build --build-arg TARGET=silkeh/clang:latest . --file Dockerfile --tag tinykvpp-clang