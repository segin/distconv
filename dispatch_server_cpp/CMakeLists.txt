cmake_minimum_required(VERSION 3.10)
project(DispatchServerCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_library(dispatch_server_core STATIC dispatch_server_core.cpp dispatch_server_core.h)

# Include directories for httplib and nlohmann/json
target_include_directories(dispatch_server_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /home/segin/distconv/third_party/json/single_include
)

# Link core library with necessary libraries
target_link_libraries(dispatch_server_core pthread)

add_executable(dispatch_server_app app_main.cpp)
target_link_libraries(dispatch_server_app dispatch_server_core)
target_include_directories(dispatch_server_app PRIVATE
    /home/segin/distconv/third_party/json/single_include
)

# Unit Tests
add_executable(dispatch_server_tests tests/main.cpp tests/jobs_post_api_tests.cpp tests/jobs_get_api_tests.cpp tests/engines_api_tests.cpp tests/jobs_status_update_tests.cpp tests/job_resubmission_tests.cpp tests/scheduling_logic_tests.cpp)
target_link_libraries(dispatch_server_tests dispatch_server_core gtest_main)
target_include_directories(dispatch_server_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /home/segin/distconv/third_party/json/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(dispatch_server_tests)