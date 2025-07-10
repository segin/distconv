cmake_minimum_required(VERSION 3.10)
project(TranscodingEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Main application
add_executable(transcoding_engine 
    app_main.cpp 
    transcoding_engine_core.cpp
)

find_package(cJSON REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(transcoding_engine cjson curl sqlite3)
target_include_directories(transcoding_engine PRIVATE /usr/include)

# Add tests subdirectory
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
