cmake_minimum_required(VERSION 3.27)
project(SuperCFGExamples)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(".." "../build/tests")
include_directories("..")
#add_library(supercfg STATIC "..")

add_executable(calc calc.cpp)
#target_link_libraries(calc supercfg)