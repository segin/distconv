cmake_minimum_required(VERSION 3.14)
project(TranscodingEngineTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# Fetch GTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fetch cJSON
FetchContent_Declare(
  cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG v1.7.15
)
FetchContent_MakeAvailable(cjson)

# Fix cjson include path to match "cjson/cJSON.h"
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/cjson)
file(COPY ${cjson_SOURCE_DIR}/cJSON.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/cjson)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

# Setup CPR (includes curl)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(BUILD_CPR_TESTS OFF CACHE BOOL "" FORCE)
set(CPR_FORCE_OPENSSL_BACKEND ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE) # Avoid libpsl issues if missing
set(CPR_USE_SYSTEM_LIB_PSL ON CACHE BOOL "" FORCE) # Trick cpr to not build libpsl

# Add CPR subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpr ${CMAKE_CURRENT_BINARY_DIR}/cpr_build)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(${gtest_SOURCE_DIR}/googletest/include)
include_directories(${cjson_SOURCE_DIR})
include_directories(..)

# Create the test executable
add_executable(transcoding_engine_tests
    main.cpp
    test_helpers.cpp
    initialization_tests.cpp
    main_loop_tests.cpp
    transcoding_process_tests.cpp
    sqlite_queue_tests.cpp
    ../transcoding_engine_core.cpp
)

# Link libraries
# Use libcurl target exported by cpr if available
if(TARGET libcurl)
    target_link_libraries(transcoding_engine_tests libcurl)
elseif(TARGET CURL::libcurl)
    target_link_libraries(transcoding_engine_tests CURL::libcurl)
endif()

target_link_libraries(transcoding_engine_tests
    GTest::gtest
    GTest::gtest_main
    cjson
    cpr::cpr
    SQLite::SQLite3
    Threads::Threads
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

# Benchmark
add_executable(benchmark_json
    benchmark_json.cpp
    ../transcoding_engine_core.cpp
)

# Use libcurl target exported by cpr if available
if(TARGET libcurl)
    target_link_libraries(benchmark_json libcurl)
elseif(TARGET CURL::libcurl)
    target_link_libraries(benchmark_json CURL::libcurl)
endif()

target_link_libraries(benchmark_json
    cjson
    cpr::cpr
    SQLite::SQLite3
    Threads::Threads
)
