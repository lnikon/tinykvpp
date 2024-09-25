#!/bin/sh

docker build --build-arg TARGET=gcc:latest . --file Dockerfile --tag tinykvpp-gcc