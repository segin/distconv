cmake_minimum_required(VERSION 3.10)
project(TranscodingEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(transcoding_engine main.cpp)

find_package(cJSON REQUIRED)

# Link with necessary libraries (e.g., curl if using a proper library)
# target_link_libraries(transcoding_engine curl)
target_link_libraries(transcoding_engine cjson)
