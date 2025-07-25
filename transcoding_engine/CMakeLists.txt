cmake_minimum_required(VERSION 3.15)
project(TranscodingEngine VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
add_compile_options(-Wall -Wextra -Wpedantic)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find cpr (try package first, fallback to manual)
find_package(cpr QUIET)
if(NOT cpr_FOUND)
    # Try to find via pkg-config
    pkg_check_modules(CPR QUIET cpr)
    if(CPR_FOUND)
        add_library(cpr::cpr INTERFACE IMPORTED)
        target_link_libraries(cpr::cpr INTERFACE ${CPR_LIBRARIES})
        target_include_directories(cpr::cpr INTERFACE ${CPR_INCLUDE_DIRS})
        target_compile_options(cpr::cpr INTERFACE ${CPR_CFLAGS_OTHER})
        set(CPR_AVAILABLE TRUE)
    else()
        message(WARNING "cpr library not found. Building without HTTP functionality.")
        # Create a dummy cpr target
        add_library(cpr::cpr INTERFACE IMPORTED)
        set(CPR_AVAILABLE FALSE)
    endif()
else()
    set(CPR_AVAILABLE TRUE)
endif()

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
    if(NLOHMANN_JSON_FOUND)
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        target_include_directories(nlohmann_json::nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "nlohmann_json not found. Please install nlohmann-json3-dev.")
    endif()
endif()

# Find SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
    if(SQLITE3_FOUND)
        add_library(SQLite::SQLite3 INTERFACE IMPORTED)
        target_link_libraries(SQLite::SQLite3 INTERFACE ${SQLITE3_LIBRARIES})
        target_include_directories(SQLite::SQLite3 INTERFACE ${SQLITE3_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "SQLite3 not found. Please install libsqlite3-dev.")
    endif()
endif()

# Find GTest for testing
find_package(GTest QUIET)

# Include directories
include_directories(src)

# Create the main library
add_library(transcoding_engine_lib
    src/implementations/cpr_http_client.cpp
    src/implementations/sqlite_database.cpp
    src/implementations/secure_subprocess.cpp
    src/core/transcoding_engine.cpp
)

target_link_libraries(transcoding_engine_lib
    cpr::cpr
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
    Threads::Threads
)

target_include_directories(transcoding_engine_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Create the main application
add_executable(transcoding_engine_modern
    app_main_modern.cpp
)

target_link_libraries(transcoding_engine_modern
    transcoding_engine_lib
)

# Create mock library for testing
add_library(transcoding_engine_mocks
    src/mocks/mock_http_client.cpp
    src/mocks/mock_database.cpp
    src/mocks/mock_subprocess.cpp
)

target_link_libraries(transcoding_engine_mocks
    transcoding_engine_lib
)

# Testing
if(GTest_FOUND)
    enable_testing()
    
    add_executable(transcoding_engine_modern_tests
        tests_modern/test_main.cpp
        tests_modern/test_transcoding_engine.cpp
        tests_modern/test_implementations.cpp
        tests_modern/test_refactoring.cpp
    )
    
    target_link_libraries(transcoding_engine_modern_tests
        transcoding_engine_lib
        transcoding_engine_mocks
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    
    # Add test discovery
    include(GoogleTest)
    gtest_discover_tests(transcoding_engine_modern_tests)
    
    # Add custom test targets
    add_custom_target(test_refactoring
        COMMAND transcoding_engine_modern_tests --gtest_filter="*Refactoring*"
        DEPENDS transcoding_engine_modern_tests
        COMMENT "Running refactoring tests"
    )
    
    add_custom_target(test_implementations
        COMMAND transcoding_engine_modern_tests --gtest_filter="*Implementation*"
        DEPENDS transcoding_engine_modern_tests
        COMMENT "Running implementation tests"
    )
    
else()
    message(WARNING "GTest not found - tests will not be built")
endif()

# Installation
install(TARGETS transcoding_engine_modern
    RUNTIME DESTINATION bin
)

install(TARGETS transcoding_engine_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/
    DESTINATION include/transcoding_engine
    FILES_MATCHING PATTERN "*.h"
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    TranscodingEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Build configuration summary
message(STATUS "TranscodingEngine build configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Testing: ${GTest_FOUND}")
message(STATUS "  CPR found: ${cpr_FOUND}")
message(STATUS "  nlohmann_json found: ${nlohmann_json_FOUND}")
message(STATUS "  SQLite3 found: ${SQLite3_FOUND}")

# Add build info
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/build_info.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/utils/build_info.h"
    @ONLY
)

target_include_directories(transcoding_engine_lib PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/src
)