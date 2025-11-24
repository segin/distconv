#include "test_common.h"

using namespace distconv::DispatchServer;

// Define static members outside the class
DispatchServer* ApiTest::server = nullptr;
httplib::Client* ApiTest::client = nullptr;
int ApiTest::port = 0;
std::string ApiTest::api_key = "test_api_key";
httplib::Headers ApiTest::admin_headers;
std::string ApiTest::persistent_storage_file;
