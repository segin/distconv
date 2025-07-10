#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include "../transcoding_engine_core.h"

class TranscodingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path = "test_transcoding_" + std::to_string(std::time(nullptr)) + ".db";
        
        // Create a temporary config for testing
        test_config.dispatch_url = "http://test-server:8080";
        test_config.engine_id = "test-engine-123";
        test_config.ca_cert_path = "test_server.crt";
        test_config.api_key = "test-api-key";
        test_config.hostname = "test-hostname";
        
        // Clean up any existing test files
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }

    void TearDown() override {
        // Clean up test database
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
        
        // Clean up any test files created during testing
        for (const auto& file : test_files_to_cleanup) {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

    struct TestConfig {
        std::string dispatch_url;
        std::string engine_id;
        std::string ca_cert_path;
        std::string api_key;
        std::string hostname;
    };

    TestConfig test_config;
    std::string test_db_path;
    std::vector<std::string> test_files_to_cleanup;

    // Helper function to create test arguments
    std::vector<char*> create_test_args(const std::vector<std::string>& args) {
        test_args_storage.clear();
        test_args_storage.reserve(args.size());
        
        for (const auto& arg : args) {
            test_args_storage.emplace_back(arg);
        }
        
        std::vector<char*> argv;
        argv.reserve(test_args_storage.size());
        for (auto& arg : test_args_storage) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        return argv;
    }

private:
    std::vector<std::string> test_args_storage;
};

// Helper function to check if a string is a valid JSON
bool is_valid_json(const std::string& json_str);

// Helper function to create a temporary file with content
std::string create_temp_file(const std::string& content, const std::string& suffix = ".tmp");

// Helper function to check if SQLite database exists and has expected schema
bool verify_sqlite_schema(const std::string& db_path);

#endif // TEST_COMMON_H