cmake_minimum_required(VERSION 3.10)

project(WisentCppUnitTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(UnitTests ${CMAKE_CURRENT_SOURCE_DIR}/GTestMain.cpp)

set(HelperFiles
  ${CMAKE_CURRENT_SOURCE_DIR}/helpers/unitTestHelpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/helpers/MockSharedMemorySegment.cpp
)

set(SourceFiles
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/Algorithms.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/LZ77.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/Huffman.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/FSE.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/Delta.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/Helpers/CompressionHelpers/RLE.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/BsonSerializer/BsonSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/WisentSerializer/WisentSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Src/WisentCompressor/WisentCompressor.cpp
)

set(TestFiles
  ${CMAKE_CURRENT_SOURCE_DIR}/TestMockSharedMemorySegment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestCsvLoading.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestCompression.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestBsonSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestWisentSerializer.cpp
)

add_library(Helpers SHARED ${HelperFiles})
add_library(Source SHARED ${SourceFiles})
add_library(Tests SHARED ${TestFiles})

target_link_libraries(UnitTests 
  PRIVATE
    Helpers
    Source
    Tests
    GTest::GTest
    GTest::Main
)

enable_testing()

add_test(NAME UnitTests COMMAND UnitTests)
