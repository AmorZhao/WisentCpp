cmake_minimum_required(VERSION 3.10)
project(wisentcpp-demo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR})
add_executable(wisent 
  ${SRC_DIR}/WisentHandlers.cpp
)

target_include_directories(wisent PRIVATE
    ${SRC_DIR}/../../Include/
)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Emscripten is required to build this project.")
else()
  set_target_properties(wisent PROPERTIES OUTPUT_NAME "wisent")
  set(CMAKE_CXX_COMPILER "em++")

  target_link_options(wisent PRIVATE
    "-sSTANDALONE_WASM=1"
    "-sMODULARIZE=1"
    "-sEXPORTED_FUNCTIONS=['_simple_add','_loadWisent','_malloc','_realloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8','HEAP8','HEAPU8','HEAP32','HEAPU32', 'HEAPF32','HEAPF64', 'FS', 'FS_createPath', 'FS_createDataFile', 'FS_analyzePath', 'FS_readFile']"
    "-sFORCE_FILESYSTEM=1"
    "-sENVIRONMENT=node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINVOKE_RUN=0"
    "--no-entry"
    "-O0"
    "-std=c++17"
  )
endif()

# Copy to client-app folder
set(OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../client-app/public/wasm)

add_custom_command(
  TARGET wisent POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/wisent.wasm
    ${CMAKE_CURRENT_BINARY_DIR}/wisent.js
    ${OUTPUT_DIR}
  COMMENT "Copying wasm output to ${OUTPUT_DIR}"
)

add_custom_target(copy_wasm_to_frontend ALL DEPENDS wisent)
