cmake_minimum_required(VERSION 3.20)
project(hello-world)

list(APPEND CMAKE_PREFIX_PATH "${LLVM_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_library(HelloWorld SHARED HelloWorld.cpp)
add_library(OpcodeCounter SHARED OpcodeCounter.cpp)