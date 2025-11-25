cmake_minimum_required(VERSION 3.10)
project(DispatchServerCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set debug flags for no optimization and debug symbols
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb3")

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(dispatch_server_core STATIC 
    dispatch_server_core.cpp dispatch_server_core.h 
    api_middleware.cpp api_middleware.h 
    repositories.cpp repositories.h 
    enhanced_endpoints.cpp enhanced_endpoints.h
    request_handlers.cpp request_handlers.h
    job_handlers.cpp job_handlers.h
    engine_handlers.cpp engine_handlers.h
    job_action_handlers.cpp job_action_handlers.h
    assignment_handler.cpp assignment_handler.h
    job_update_handler.cpp job_update_handler.h
    storage_pool_handler.cpp storage_pool_handler.h
    server_config.cpp server_config.h
    response_utils.cpp response_utils.h
    dispatch_server_constants.h
    job_publisher.cpp job_publisher.h
    status_subscriber.cpp status_subscriber.h
    message_queue.h
    memory_message_queue.h
)

# Include directories for httplib and nlohmann/json
target_include_directories(dispatch_server_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../third_party/json/single_include
    ${CMAKE_SOURCE_DIR}/../third_party/trompeloeil/include
)

# Find UUID library
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# Link core library with necessary libraries
target_link_libraries(dispatch_server_core pthread sqlite3 ${UUID_LIBRARIES})

add_executable(dispatch_server_app app_main.cpp)
target_link_libraries(dispatch_server_app dispatch_server_core)
target_include_directories(dispatch_server_app PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/json/single_include
)

# Modern app with dependency injection
add_executable(dispatch_server_modern app_main_modern.cpp)
target_link_libraries(dispatch_server_modern dispatch_server_core)
target_include_directories(dispatch_server_modern PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/json/single_include
)

# Unit Tests
add_executable(dispatch_server_tests tests/main.cpp tests/jobs_post_api_tests.cpp tests/jobs_get_api_tests.cpp tests/engines_api_tests.cpp tests/jobs_status_update_tests.cpp tests/job_resubmission_tests.cpp tests/scheduling_logic_tests.cpp tests/persistent_storage_tests.cpp tests/jobs_assign_api_tests.cpp tests/job_state_transition_tests.cpp tests/command_line_tests.cpp tests/thread_safety_tests.cpp tests/api_test_definitions.cpp tests/json_parsing_edge_case_tests.cpp tests/final_edge_case_tests.cpp tests/thread_safety_improvements_tests.cpp tests/job_handlers_tests.cpp tests/engine_handlers_tests.cpp tests/job_management_tests.cpp tests/api_endpoint_tests.cpp tests/error_handling_tests.cpp tests/url_validation_tests.cpp tests/server_config_tests.cpp tests/max_retries_validation_tests.cpp tests/message_queue_tests.cpp tests/message_queue_integration_tests.cpp)
target_link_libraries(dispatch_server_tests dispatch_server_core gtest gmock)
target_include_directories(dispatch_server_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../third_party/json/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/../third_party/trompeloeil/include
)

# Optimized dedicated test target for Message Queue
add_executable(mq_tests
    tests/main.cpp
    tests/message_queue_tests.cpp
    tests/message_queue_integration_tests.cpp
)
target_link_libraries(mq_tests dispatch_server_core gtest gmock)
target_include_directories(mq_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../third_party/json/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/../third_party/trompeloeil/include
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(dispatch_server_tests
    PROPERTIES
    TIMEOUT 10
)
gtest_discover_tests(mq_tests
    PROPERTIES
    TIMEOUT 10
)
