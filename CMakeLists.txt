cmake_minimum_required(VERSION 3.10)

# ---- Project ----

project(
  WisentCpp
  VERSION 1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
# set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

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

# CPMAddPackage(
#   NAME fmt
#   GIT_TAG 10.2.1
#   GITHUB_REPOSITORY fmtlib/fmt
#   OPTIONS "FMT_INSTALL YES" # create an installable target
# )

# ---- Add source files ----

include_directories(${CMAKE_SOURCE_DIR}/Include)
include_directories(${CMAKE_SOURCE_DIR}/Source)

set(WisentServerFiles Source/WisentServer.cpp)
set(WisentSerializerFiles Source/WisentSerializer.cpp Source/SharedMemorySegment.cpp)
set(BsonSerializerFiles Source/BsonSerializer.cpp)

# WisentSerializer Plugin
add_library(WisentSerializer SHARED ${WisentSerializerFiles})

# Wisent Server
add_executable(WisentServer ${WisentSerializerFiles} ${BsonSerializerFiles} ${WisentServerFiles})
# add_dependencies(WisentServer httplib)

# # Benchmarks
# add_executable(Benchmarks ${WisentSerializerFiles} ${BsonSerializerFiles} ${WisentBenchmarkFiles})
# add_dependencies(Benchmarks googlebenchmark)
# target_link_libraries(Benchmarks PRIVATE ITTNotifySupport)
# target_link_libraries(Benchmarks PRIVATE benchmark)
# target_link_libraries(Benchmarks PRIVATE benchmark_main)
# target_compile_options(Benchmarks PUBLIC -DBENCHMARK_STATIC_DEFINE)
# add_dependencies(Benchmarks cpp-httplib)
# add_dependencies(Benchmarks rapidjson)

list(APPEND AllExeTargets WisentServer)
list(APPEND AllTargets WisentServer WisentSerializer)

foreach(Target IN LISTS AllTargets)
    set_target_properties(${Target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options(${Target} PUBLIC "/Zc:__cplusplus")
        target_compile_options(${Target} PUBLIC "/EHsc")
    endif()
    set_target_properties(${Target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_include_directories(${Target} SYSTEM PUBLIC ${WisentBenchmarks_BINARY_DIR}/deps/include)
    target_link_directories(${Target} PUBLIC ${WisentBenchmarks_BINARY_DIR}/deps/lib)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${Target} PRIVATE rt)
    endif()
    target_include_directories(${Target} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_libraries(${Target} PRIVATE ${CMAKE_SOURCE_DIR}/Include)
    # target_link_libraries(${Target} PRIVATE nlohmann_json)
    # target_link_libraries(${Target} PRIVATE simdjson)
endforeach()

set_target_properties(WisentSerializer PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(TARGETS WisentSerializer LIBRARY DESTINATION lib)
install(TARGETS WisentServer RUNTIME DESTINATION bin)