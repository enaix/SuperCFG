cmake_minimum_required(VERSION 3.27)
project(SuperCFG)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SUPERCFG_FILES base/*.h base/*.cpp cfg/*.h cfg/*.cpp elemtree/*.h elemtree/*.cpp)

include_directories(".")

add_executable(SuperCFG tests/main.cpp ${SUPERCFG_FILES})
