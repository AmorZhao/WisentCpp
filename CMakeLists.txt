cmake_minimum_required(VERSION 3.10)

project(
  WisentCpp
  VERSION 1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fstandalone-debug")
# set(CMAKE_CXX_EXTENSIONS NO)

find_program(CCACHE_EXECUTABLE ccache)
mark_as_advanced(CCACHE_EXECUTABLE)
if(CCACHE_EXECUTABLE)
  foreach(LANG C CXX)
    if(NOT DEFINED CMAKE_${LANG}_COMPILER_LAUNCHER AND NOT CMAKE_${LANG}_COMPILER MATCHES ".*/ccache")
      message(STATUS "Enabling ccache for ${LANG}")
      set(CMAKE_${LANG}_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "")
    endif()
  endforeach()
endif()

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

add_executable(WisentServer Src/WisentServer.cpp)

set(HelperFiles 
  Src/Helpers/SharedMemorySegment.cpp 
  Src/Helpers/ISharedMemorySegment.hpp
  Src/CompressionHelpers/FiniteStateEntropy.cpp
  Src/CompressionHelpers/Huffman.cpp
  Src/CompressionHelpers/LZ77.cpp
)

set(SourceFiles
  Src/WisentSerializer/WisentHelpers.h
  Src/WisentSerializer/WisentSerializer.cpp
  Src/WisentParser/WisentParser.cpp
  Src/WisentCompressor/WisentCompressor.cpp
  Src/BsonSerializer/BsonSerializer.cpp
)

add_library(Helpers SHARED ${HelperFiles})
add_library(Source SHARED ${SourceFiles})
target_link_libraries(WisentServer PRIVATE Helpers Source)

install(TARGETS Helpers Source LIBRARY DESTINATION lib)
install(TARGETS WisentServer RUNTIME DESTINATION bin)
