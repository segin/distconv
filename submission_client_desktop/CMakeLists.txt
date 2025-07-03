cmake_minimum_required(VERSION 3.10)
project(SubmissionClientDesktop)

set(CMAKE_CXX_STANDARD 17)

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# Add third-party libraries
add_subdirectory(../third_party/cpr ${CMAKE_BINARY_DIR}/cpr)
add_subdirectory(../third_party/json ${CMAKE_BINARY_DIR}/json)
add_subdirectory(../third_party/trompeloeil ${CMAKE_BINARY_DIR}/trompeloeil)

# Fetch Google Test for the tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Create the core library
add_library(submission_client_core submission_client_core.cpp)

# The core library needs to know where to find the headers for its dependencies.
# By making them PUBLIC, any target that links against submission_client_core
# will also inherit these include directories.
target_include_directories(submission_client_core
    PUBLIC
        ${wxWidgets_INCLUDE_DIRS}
        ../third_party/cpr/include
        ../third_party/json/include
        ${CMAKE_BINARY_DIR}/cpr/include # For generated cprver.h
)

target_link_libraries(submission_client_core
    PUBLIC
        ${wxWidgets_LIBRARIES}
        cpr::cpr
        nlohmann_json::nlohmann_json
)

# Create the executable for the submission client
add_executable(submission_client_desktop app_main.cpp)
target_link_libraries(submission_client_desktop PRIVATE submission_client_core)

# Create the executable for the tests
enable_testing()
add_executable(submission_client_desktop_tests tests/main.cpp)
target_include_directories(submission_client_desktop_tests PRIVATE ../third_party/trompeloeil/include)
target_link_libraries(submission_client_desktop_tests PRIVATE gtest_main submission_client_core)