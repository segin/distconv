cmake_minimum_required(VERSION 3.10)
project(SubmissionClientDesktop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add cpr as a subdirectory
add_subdirectory(../../third_party/cpr EXCLUDE_FROM_ALL)

# Add nlohmann/json as a subdirectory
add_subdirectory(../../third_party/json EXCLUDE_FROM_ALL)

add_executable(submission_client_desktop main.cpp)

target_link_libraries(submission_client_desktop cpr nlohmann_json)
