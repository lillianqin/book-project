cmake_minimum_required(VERSION 3.20)

project(bookproj
        VERSION 0.0
        DESCRIPTION "Lillian's Book Experiment"
        LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(bookproj_compiler_flags INTERFACE)
target_compile_features(bookproj_compiler_flags INTERFACE cxx_std_23)
target_compile_options(bookproj_compiler_flags INTERFACE "-Werror;-Wall")

enable_testing()

add_subdirectory(message)
add_subdirectory(itch50)
add_subdirectory(orderbook)
add_subdirectory(datasource)
add_subdirectory(digest)

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(3party/abseil-cpp)
add_subdirectory(3party/mimalloc)
add_subdirectory(3party/tlx)
add_subdirectory(examples)

add_dependencies(itch50 message orderbook datasource digest)
