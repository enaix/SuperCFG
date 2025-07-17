cmake_minimum_required(VERSION 3.27)
project(SuperCFG)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -Werror=return-type")
endif ()

file(GLOB_RECURSE SUPERCFG_FILES base/*.h base/*.cpp cfg/*.h cfg/*.cpp elemtree/*.h elemtree/*.cpp extra/*.h)

include_directories(".")
include_directories("simply-curse/lib/")

add_subdirectory(simply-curse)

add_executable(SuperCFG tests/main.cpp ${SUPERCFG_FILES})
add_executable(superdbg extra/dbg.cpp ${SUPERCFG_FILES} simply-curse/lib/curse.h)

target_link_libraries(superdbg INTERFACE curse)