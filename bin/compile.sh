#!/bin/bash 

rm -rf CMakeCache.txt  CMakeFiles  cmake_install.cmake  Makefile  logger logs/*
cmake ..
make -j
# ./logger