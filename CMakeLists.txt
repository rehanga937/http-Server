cmake_minimum_required(VERSION 3.13)
project(http-server-starter-cpp)
set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

#add_compile_options(-lpthread) - didn't work for codecrafters
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

set(SOURCE_FILES src/server.cpp)

add_executable(server ${SOURCE_FILES})
