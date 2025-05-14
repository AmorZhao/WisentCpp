# Compiles the C++ code to WebAssembly using Emscripten.
# Successfully compiled wasm file is under `client-app/public/wasm`

rm -rf compiled

mkdir compiled
cd compiled

emcmake cmake .. -DCMAKE_BUILD_TYPE=Debug

cmake --build .

if [ $? -eq 0 ]; then
    cd ..
    rm -rf compiled
fi