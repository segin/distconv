cmake_minimum_required(VERSION 3.10)
project(TranscodingEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(transcoding_engine app_main.cpp)

find_package(cJSON REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(transcoding_engine cjson curl sqlite3)
target_include_directories(transcoding_engine PRIVATE /usr/include)
