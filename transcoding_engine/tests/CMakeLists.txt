cmake_minimum_required(VERSION 3.10)
project(TranscodingEngineTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(GTest REQUIRED)
find_package(cJSON REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(..)

# Create the test executable
add_executable(transcoding_engine_tests
    main.cpp
    test_helpers.cpp
    initialization_tests.cpp
    ../transcoding_engine_core.cpp
)

# Link libraries
target_link_libraries(transcoding_engine_tests
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    cjson
    curl
    sqlite3
    Threads::Threads
    gtest
    gtest_main
    pthread
)

# Add compiler flags for warnings
target_compile_options(transcoding_engine_tests PRIVATE 
    -Wall -Wextra -pedantic
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Add test discovery
include(GoogleTest)
gtest_discover_tests(transcoding_engine_tests)