cmake_minimum_required(VERSION 3.10)

project(WisentCppBenchmarks)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

find_package(benchmark REQUIRED)

set(Src_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Src/")
set(VTUNE_ROOT /opt/intel/oneapi/vtune/latest)
set(ENABLE_VTUNE ON)

include_directories(${VTUNE_ROOT}/include)
link_directories(${VTUNE_ROOT}/lib64)

add_executable(Benchmarks ${CMAKE_CURRENT_SOURCE_DIR}/benchmark.cpp)

option(ENABLE_VTUNE "Enable VTune Profiling" ON)
if(ENABLE_VTUNE)
    target_compile_definitions(Benchmarks PRIVATE ENABLE_VTUNE)
    target_link_libraries(Benchmarks PRIVATE ${VTUNE_ROOT}/lib64/libittnotify.a)
endif()

set(HelperFiles
    ${Src_DIR}/Helpers/SharedMemorySegment.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/Algorithms.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/BitPacking.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/RLE.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/Delta.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/LZ77.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/Huffman.cpp
    ${Src_DIR}/Helpers/CompressionHelpers/FSE.cpp
)

set(SourceFiles
    # ${Src_DIR}/BsonSerializer/BsonSerializer.cpp
    ${Src_DIR}/WisentSerializer/WisentSerializer.cpp
    ${Src_DIR}/WisentCompressor/WisentCompressor.cpp
)

set(BenchmarkFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/utilities.cpp
)

add_library(Helpers SHARED ${HelperFiles})
add_library(Source SHARED ${SourceFiles})
add_library(BenchmarksLib SHARED ${BenchmarkFiles})

target_link_libraries(Benchmarks
  PRIVATE 
    Helpers 
    Source 
    BenchmarksLib
    benchmark::benchmark
    ${Boost_FILESYSTEM_LIBRARY}
    pthread
)

install(TARGETS Helpers Source BenchmarksLib LIBRARY DESTINATION lib)
install(TARGETS Benchmarks RUNTIME DESTINATION bin)
