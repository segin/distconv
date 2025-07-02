cmake_minimum_required(VERSION 3.10)
project(DispatchServerCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(dispatch_server_core STATIC dispatch_server_core.cpp dispatch_server_core.h)

# Include directories for httplib and nlohmann/json
target_include_directories(dispatch_server_core PRIVATE
    /home/segin/distconv/third_party/httplib
    /home/segin/distconv/third_party/json/single_include
)

# Link core library with necessary libraries
target_link_libraries(dispatch_server_core pthread)

add_executable(dispatch_server_app app_main.cpp)
target_link_libraries(dispatch_server_app dispatch_server_core)
target_include_directories(dispatch_server_app PRIVATE
    /home/segin/distconv/third_party/json/single_include
)


