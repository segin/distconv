#include "gtest/gtest.h"
#include "../server_config.h"

using namespace distconv::DispatchServer;

class ServerConfigTest : public ::testing::Test {
protected:
    // Helper to create argv
    // We don't need a complex helper if we just do it in each test
    // or we can use a member to hold strings
    std::vector<std::string> args_storage;
    
    void SetUp() override {
        args_storage.clear();
    }
};

TEST(ServerConfigTest, ParsesValidArguments) {
    std::vector<std::string> args = {"program", "--api-key", "secret", "--port", "9090", "--database", "test.db"};
    std::vector<char*> argv;
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    
    ServerConfig config = parse_arguments(argv.size(), argv.data());
    
    EXPECT_FALSE(config.parse_error);
    EXPECT_EQ(config.api_key, "secret");
    EXPECT_EQ(config.port, 9090);
    EXPECT_EQ(config.database_path, "test.db");
    EXPECT_FALSE(config.show_help);
}

TEST(ServerConfigTest, HandlesInvalidPort) {
    std::vector<std::string> args = {"program", "--port", "invalid"};
    std::vector<char*> argv;
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    
    ServerConfig config = parse_arguments(argv.size(), argv.data());
    
    EXPECT_TRUE(config.parse_error);
    EXPECT_NE(config.error_message.find("Invalid port number"), std::string::npos);
}

TEST(ServerConfigTest, HandlesHelpFlag) {
    std::vector<std::string> args = {"program", "--help"};
    std::vector<char*> argv;
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    
    ServerConfig config = parse_arguments(argv.size(), argv.data());
    
    EXPECT_TRUE(config.show_help);
}

TEST(ServerConfigTest, HandlesMissingValues) {
    // This depends on implementation, but currently it might just skip or take next flag
    // The current implementation checks i + 1 < argc
    std::vector<std::string> args = {"program", "--port"};
    std::vector<char*> argv;
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    
    ServerConfig config = parse_arguments(argv.size(), argv.data());
    
    // Should remain default because check fails
    EXPECT_EQ(config.port, 8080);
}
