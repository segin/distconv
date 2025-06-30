cmake_minimum_required(VERSION 3.10)
project(DispatchServerCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(dispatch_server_cpp main.cpp)

# Include directories for httplib and nlohmann/json (using absolute paths for debugging)
include_directories(/home/segin/distconv/third_party/httplib)
include_directories(/home/segin/distconv/third_party/json/single_include)

# Link with necessary libraries (e.g., pthread for multi-threading in httplib)
find_package(OpenSSL REQUIRED)
target_link_libraries(dispatch_server_cpp pthread ${OpenSSL_LIBRARIES})