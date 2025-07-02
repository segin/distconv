cmake_minimum_required(VERSION 3.10)
project(SubmissionClientDesktop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(wxWidgets REQUIRED COMPONENTS core base)
find_package(CURL REQUIRED)

add_library(submission_client_core STATIC submission_client_core.cpp)

target_include_directories(submission_client_core PRIVATE
    /home/segin/distconv/third_party/cpr/include
    /home/segin/distconv/third_party/json/single_include
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(submission_client_core cpr nlohmann_json ${wxWidgets_LIBRARIES} CURL::CURL)

add_executable(submission_client_desktop_app app_main.cpp)
target_link_libraries(submission_client_desktop_app submission_client_core)

find_package(GTest REQUIRED)
add_executable(submission_client_desktop_tests tests/main.cpp)
target_link_libraries(submission_client_desktop_tests gtest gtest_main submission_client_core)
target_include_directories(submission_client_desktop_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})