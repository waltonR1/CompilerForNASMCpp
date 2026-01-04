cmake_minimum_required(VERSION 3.16)
project(CompilerForNASMCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES ${SRC_DIR}/*.cpp)
# Remove old handwritten parser - we use Bison parser now
list(REMOVE_ITEM SOURCES ${SRC_DIR}/parser.cpp)

flex_target(scanner ${SRC_DIR}/scanner.l ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp)
bison_target(parser ${SRC_DIR}/parser.yy ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp)
add_flex_bison_dependency(scanner parser)

add_executable(compiler
    ${SOURCES}
    ${FLEX_scanner_OUTPUTS}
    ${BISON_parser_OUTPUTS}
)

target_include_directories(compiler PRIVATE ${INC_DIR} ${CMAKE_CURRENT_BINARY_DIR})


