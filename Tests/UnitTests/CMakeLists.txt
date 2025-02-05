cmake_minimum_required(VERSION 3.10)

project(WisentCppUnitTests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

find_package(Boost REQUIRED)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

include_directories(${Boost_INCLUDE_DIRS})
set(SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/BsonSerializer/BsonSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/Helpers/SharedMemorySegment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/Helpers/CsvLoading.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/WisentSerializer/WisentSerializer.cpp

)

set(TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/GTestMain.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestSharedMemorySegment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestCsvLoading.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestBsonSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestWisentSerializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/helpers/unitTestHelpers.cpp
)

add_executable(UnitTests ${SOURCE_FILES} ${TEST_SOURCES})

target_link_libraries(UnitTests GTest::GTest GTest::Main ${Boost_LIBRARIES})

enable_testing()

add_test(NAME UnitTests COMMAND UnitTests)