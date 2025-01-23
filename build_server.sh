#!/bin/bash
cd /root/Wisent++
rm -rf build

mkdir -p build
cd build

cmake ..
cmake --build . --verbose

../build/WisentServer