cmake_minimum_required(VERSION 3.20)
project(matching_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
include(FetchContent)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Core matching library (no network dependency)
add_library(matching_lib STATIC
    src/order_book.cpp
    src/matching_engine.cpp
    src/helper.cpp
)
target_include_directories(matching_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Server executable
add_executable(matching_server
    src/main.cpp
    # src/server.cpp
)
target_link_libraries(matching_server PRIVATE matching_lib Crow::Crow nlohmann_json::nlohmann_json)