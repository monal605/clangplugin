cmake_minimum_required(VERSION 3.16)
project(AsmValidator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(AsmValidator SHARED
    plugin/AsmValidator.cpp
    plugin/AsmTokenizer.cpp
)

if(MSVC)
    target_compile_options(AsmValidator PRIVATE /GR-)
else()
    target_compile_options(AsmValidator PRIVATE -fno-rtti)
endif()

target_link_libraries(AsmValidator PRIVATE clangAST clangFrontend clangBasic)
