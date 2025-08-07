cmake_minimum_required(VERSION 3.27)
project(SuperCFG)

set(CMAKE_CXX_STANDARD 20)

# Make template messages more readable
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-show-template-tree -ftemplate-backtrace-limit=1")

option(SUPERCFG_DIAG "Enable Clang diagnostics for better template type info" OFF)

# SUPERCFG DIAG
if (SUPERCFG_DIAG)
    find_package(LLVM 22.0 REQUIRED EXACT)
    find_package(Clang 22.0 REQUIRED EXACT)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_CMAKE_DIR}")

    #llvm_map_components_to_libnames(llvm_libs support core)
    include(AddLLVM)

    add_definitions(${LLVM_DEFINITIONS})
    add_definitions(${CLANG_DEFINITIONS})

    include_directories(${LLVM_INCLUDE_DIRS})
    include_directories(${CLANG_INCLUDE_DIRS})

    link_directories("/usr/local/lib/lib")

    #set(EXTRA_DIAG_FILES ${llvm_libs} ${clang_libs})
    # set(EXTRA_DIAG_FILES clangTooling)
    set(EXTRA_DIAG_FILES clangBasic clangFrontend clangAST clangSema clangAnalysis clangEdit clangLex clangDriver clangSerialization clangParse ${llvm_libs} LLVMSupport)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_SUPERCFG_DIAG")
else ()
     set(EXTRA_DIAG_FILES)
endif ()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -Werror=return-type")
endif ()

file(GLOB_RECURSE SUPERCFG_FILES base/*.h base/*.cpp cfg/*.h cfg/*.cpp elemtree/*.h elemtree/*.cpp extra/*.h)

include_directories(".")
include_directories("simply-curse/lib/")

add_subdirectory(simply-curse)

add_executable(SuperCFG tests/main.cpp ${SUPERCFG_FILES})
add_executable(superdbg extra/dbg.cpp ${SUPERCFG_FILES} simply-curse/lib/curse.h)

target_link_libraries(SuperCFG PRIVATE ${EXTRA_DIAG_FILES})
target_link_libraries(superdbg PRIVATE ${EXTRA_DIAG_FILES})
