cmake_minimum_required(VERSION 3.10)

# ---- Project ----

project(
  WisentCpp
  VERSION 1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fstandalone-debug")
# set(CMAKE_CXX_EXTENSIONS NO)

# Configure CCache if available
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

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

# include(cmake/CPM.cmake)
# CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.8.0")

# ---- Add source files ----

include_directories(${CMAKE_SOURCE_DIR}/Include)
include_directories(${CMAKE_SOURCE_DIR}/Source)

set(WisentServerFiles Source/WisentServer.cpp)
set(WisentSerializerFiles Source/WisentSerializer.cpp Source/SharedMemorySegment.cpp)
set(BsonSerializerFiles Source/BsonSerializer.cpp)

# WisentSerializer Plugin
add_library(WisentSerializer SHARED ${WisentSerializerFiles})

# WisentServer
add_executable(WisentServer ${WisentServerFiles} ${WisentSerializerFiles} ${BsonSerializerFiles})

# Link libraries
target_link_libraries(WisentServer WisentSerializer)

# Set properties for all targets
set_target_properties(WisentServer PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
set_target_properties(WisentSerializer PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

# Install targets
install(TARGETS WisentSerializer LIBRARY DESTINATION lib)
install(TARGETS WisentServer RUNTIME DESTINATION bin)