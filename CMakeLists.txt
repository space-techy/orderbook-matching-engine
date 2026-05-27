cmake_minimum_required(VERSION 3.20)

project(matching_engine
        VERSION 0.1.0
        DESCRIPTION "IICPC Matching Engine"
        LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library
add_library(matching_lib STATIC
    src/order_book.cpp
    src/matching_engine.cpp
    src/helper.cpp
)

target_include_directories(matching_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Executable
add_executable(matching_engine_app
    src/main.cpp
)

target_link_libraries(matching_engine_app PRIVATE matching_lib)