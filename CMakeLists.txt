cmake_minimum_required(VERSION 3.10)

# Project name
project(TradingClient)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Include WebSocket++ and JSON from system paths
find_package(nlohmann_json REQUIRED)
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_client.hpp)

if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(FATAL_ERROR "WebSocket++ not found. Install it with 'sudo apt install libwebsocketpp-dev'.")
endif()

# Add include directories
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${WEBSOCKETPP_INCLUDE_DIR})

# Source files
set(SOURCES
    src/main.cpp
)

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
set(LINK_LIBS
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})

# Provide a clear message if dependencies are not found
if(NOT CURL_FOUND)
    message(FATAL_ERROR "cURL not found. Install cURL before building.")
endif()

if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found. Install OpenSSL before building.")
endif()

message(STATUS "Dependencies detected. You are ready to build!")