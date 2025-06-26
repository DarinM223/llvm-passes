cmake_minimum_required(VERSION 3.28)
project(hello-world)

list(APPEND CMAKE_PREFIX_PATH "${LLVM_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

file(
    GLOB_RECURSE HEADERS
    kaleidoscope/lexer.h
    kaleidoscope/ast.h
    kaleidoscope/parser.h
)
file(
    GLOB_RECURSE SOURCES
    kaleidoscope/main.cpp
    kaleidoscope/lexer.cpp
    kaleidoscope/parser.cpp
)

add_executable(kaleidoscope-exe ${HEADERS} ${SOURCES})
add_library(HelloWorld SHARED HelloWorld.cpp)
add_library(OpcodeCounter SHARED OpcodeCounter.cpp)
add_library(InjectFuncCall SHARED InjectFuncCall.cpp)
add_library(StaticCallCounter SHARED StaticCallCounter.cpp)
add_library(DynamicCallCounter SHARED DynamicCallCounter.cpp)
add_library(MBASub SHARED MBASub.cpp)
add_library(MBAAdd SHARED MBAAdd.cpp)
add_library(RIV SHARED RIV.cpp)
add_library(DuplicateBB SHARED DuplicateBB.cpp)
add_library(MergeBB SHARED MergeBB.cpp)
add_library(FindFCmpEq SHARED FindFCmpEq.cpp)
add_library(ConvertFCmpEq SHARED ConvertFCmpEq.cpp)

target_compile_features(DuplicateBB PUBLIC cxx_std_20)
target_compile_features(MergeBB PUBLIC cxx_std_20)