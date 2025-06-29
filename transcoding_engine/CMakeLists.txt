cmake_minimum_required(VERSION 3.10)
project(transcoding_engine)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(../../third_party/cpr)
add_subdirectory(../../third_party/json)

add_executable(transcoding_engine main.cpp)

target_link_libraries(transcoding_engine PRIVATE cpr::cpr nlohmann_json::nlohmann_json)