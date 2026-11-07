cmake_minimum_required(VERSION 3.15)
project(AdaptiveCDNRouter VERSION 1.0 LANGUAGES CXX)

# Enforce standard C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directory for headers
include_directories(include)

# Define source files
set(SOURCES
    src/Network.cpp
    src/Simulator.cpp
    src/CostCalculator.cpp
    src/Graph.cpp
    src/Algorithms.cpp
    src/FailureHandler.cpp
    src/Router.cpp
    src/EdmondsKarp.cpp
    src/CacheOptimizer.cpp
    src/DemandForecaster.cpp
    src/BaselineRouters.cpp
    main.cpp
)

# Create the main executable target
add_executable(cdn_router ${SOURCES})

# Target-specific include directories
target_include_directories(cdn_router PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler warnings
if(MSVC)
    target_compile_options(cdn_router PRIVATE /W4 /permissive-)
else()
    target_compile_options(cdn_router PRIVATE -Wall -Wextra -Wpedantic)
endif()
