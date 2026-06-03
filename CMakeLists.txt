cmake_minimum_required(VERSION 3.20)
project(matching_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler Warnings Configuration
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-29-0
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "" FORCE)


set(CROW_INSTALL OFF CACHE INTERNAL "")
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG v4.2.3
)
FetchContent_MakeAvailable(glaze)

# Core matching library
add_library(matching_lib STATIC
    src/order_book.cpp
    src/matching_engine.cpp
    src/helper.cpp
    src/server.cpp
)
target_include_directories(matching_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link your library targets to all required dependencies
target_link_libraries(matching_lib 
    PUBLIC
        glaze::glaze # Provides JSON parsing structures
        Crow::Crow   # Core web/websocket server engine
)

# Server executable
add_executable(matching_server
    src/main.cpp
)
target_link_libraries(matching_server PRIVATE matching_lib)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
