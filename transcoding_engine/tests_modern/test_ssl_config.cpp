#include <gtest/gtest.h>
#include "implementations/cpr_http_client.h"
#include <filesystem>
#include <iostream>

using namespace distconv::TranscodingEngine;

class SSLConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(SSLConfigTest, SSLOptionsConfiguration) {
    CprHttpClient client;

    // Test 1: Verify valid options (empty path, no verify) don't crash
    client.set_ssl_options("", false);
    auto response = client.get("https://example.com");
    // We don't enforce success/failure here, just that it runs.
    // The environment might have internet access, so it might succeed.

    // Test 2: Verify setting a bad CA path causes an error (proving the option is used)
    std::string bad_ca_path = "/non/existent/ca.pem";
    client.set_ssl_options(bad_ca_path, true);
    response = client.get("https://example.com");

    EXPECT_FALSE(response.success);
    // Verify the error message contains something about the certificate file
    // "error setting certificate file" is the typical curl error for bad path
    EXPECT_TRUE(response.error_message.find("certificate") != std::string::npos ||
                response.error_message.find("SSL") != std::string::npos ||
                response.error_message.find("PEM") != std::string::npos);

    std::cout << "[SSLConfigTest] Response error with bad CA path: " << response.error_message << std::endl;
}
