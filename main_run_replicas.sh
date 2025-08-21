#!/bin/bash

./build/Main -c ./assets/tkvpp_config_1.json &> log_1.txt &
./build/Main -c ./assets/tkvpp_config_2.json &> log_2.txt &
./build/Main -c ./assets/tkvpp_config_3.json &> log_3.txt &
