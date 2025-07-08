cmake_minimum_required(VERSION 3.27)
project(SuperCFG)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -Werror=return-type")
endif ()

file(GLOB_RECURSE SUPERCFG_FILES base/*.h base/*.cpp cfg/*.h cfg/*.cpp elemtree/*.h elemtree/*.cpp extra/*.h extra/*.cpp)

include_directories(".")

add_executable(SuperCFG tests/main.cpp ${SUPERCFG_FILES})
add_executable(ui_test tests/ui.cpp ${SUPERCFG_FILES})