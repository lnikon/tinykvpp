#!/bin/bash

./build/RaftMain --id 1 --nodes 0.0.0.0:8080,0.0.0.0:8081,0.0.0.0:8082 &> log_1.txt &
./build/RaftMain --id 2 --nodes 0.0.0.0:8080,0.0.0.0:8081,0.0.0.0:8082 &> log_2.txt &
./build/RaftMain --id 3 --nodes 0.0.0.0:8080,0.0.0.0:8081,0.0.0.0:8082 &> log_3.txt &
