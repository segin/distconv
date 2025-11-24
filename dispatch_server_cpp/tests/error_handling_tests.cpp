#include <gtest/gtest.h>
#include "request_handlers.h"
#include "httplib.h"

using namespace distconv::DispatchServer;

// Test JSON error response format
TEST(ErrorHandlingTest, JSONErrorResponseFormat) {
    httplib::Response res;
    
    set_json_error_response(res, "Test error message", "test_error_type", 400, "Additional details");
    
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(res.get_header_value("Content-Type"), "application/json");
    
    nlohmann::json error_obj = nlohmann::json::parse(res.body);
    EXPECT_EQ(error_obj["error"], "Test error message");
    EXPECT_EQ(error_obj["error_type"], "test_error_type");
    EXPECT_EQ(error_obj["status"], 400);
    EXPECT_EQ(error_obj["details"], "Additional details");
}

// Test JSON error response without details
TEST(ErrorHandlingTest, JSONErrorResponseWithoutDetails) {
    httplib::Response res;
    
    set_json_error_response(res, "Simple error", "simple_error", 404);
    
    EXPECT_EQ(res.status, 404);
    
    nlohmann::json error_obj = nlohmann::json::parse(res.body);
    EXPECT_EQ(error_obj["error"], "Simple error");
    EXPECT_EQ(error_obj["error_type"], "simple_error");
    EXPECT_EQ(error_obj["status"], 404);
    EXPECT_FALSE(error_obj.contains("details") && !error_obj["details"].empty());
}

// Test that error responses are valid JSON
TEST(ErrorHandlingTest, ErrorResponsesAreValidJSON) {
    httplib::Response res;
    
    set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, 
                           "parse error at position 5");
    
    // Should not throw when parsing
    EXPECT_NO_THROW({
        nlohmann::json error = nlohmann::json::parse(res.body);
        EXPECT_TRUE(error.is_object());
        EXPECT_TRUE(error.contains("error"));
        EXPECT_TRUE(error.contains("error_type"));
        EXPECT_TRUE(error.contains("status"));
    });
}
// Test set_json_response helper
TEST(ErrorHandlingTest, HelperSetJSONResponse) {
    httplib::Response res;
    nlohmann::json data = {{"key", "value"}, {"number", 123}};
    
    set_json_response(res, data, 201);
    
    EXPECT_EQ(res.status, 201);
    EXPECT_EQ(res.get_header_value("Content-Type"), "application/json");
    
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_EQ(parsed["key"], "value");
    EXPECT_EQ(parsed["number"], 123);
}

// Test set_error_response helper (legacy text/plain)
TEST(ErrorHandlingTest, HelperSetErrorResponse) {
    httplib::Response res;
    
    set_error_response(res, "Legacy error message", 500);
    
    EXPECT_EQ(res.status, 500);
    EXPECT_EQ(res.get_header_value("Content-Type"), "text/plain");
    EXPECT_EQ(res.body, "Legacy error message");
}

// Test JSON error response with special characters
TEST(ErrorHandlingTest, JSONErrorResponseWithSpecialChars) {
    httplib::Response res;
    std::string special_chars = "Error with \"quotes\", \n newlines, and \t tabs";
    
    set_json_error_response(res, special_chars, "special_char_error", 400);
    
    EXPECT_EQ(res.status, 400);
    
    nlohmann::json error_obj = nlohmann::json::parse(res.body);
    EXPECT_EQ(error_obj["error"], special_chars);
    EXPECT_EQ(error_obj["error_type"], "special_char_error");
}
