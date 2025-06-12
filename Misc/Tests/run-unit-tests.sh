#!/bin/bash

cd "$(dirname "$0")/UnitTests"
rm -rf build
mkdir -p build
cd build

cmake ..
make

ctest
./UnitTests