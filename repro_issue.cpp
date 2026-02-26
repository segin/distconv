#define CPPHTTPLIB_BROTLI_SUPPORT
#include "dispatch_server_cpp/httplib.h"
#include <iostream>
#include <cassert>

int main() {
    httplib::Request req;
    httplib::Response res;

    res.set_header("Content-Type", "text/plain");

    // Test case 1: br;q=0 should NOT be Brotli
    req.set_header("Accept-Encoding", "br;q=0");
    auto type = httplib::detail::encoding_type(req, res);

    // Current behavior (bug): returns Brotli
    if (type == httplib::detail::EncodingType::Brotli) {
        std::cout << "FAIL: br;q=0 resulted in Brotli" << std::endl;
    } else {
        std::cout << "PASS: br;q=0 did not result in Brotli" << std::endl;
    }

    // Test case 2: br should be Brotli
    req.set_header("Accept-Encoding", "br");
    type = httplib::detail::encoding_type(req, res);
    if (type == httplib::detail::EncodingType::Brotli) {
        std::cout << "PASS: br resulted in Brotli" << std::endl;
    } else {
        std::cout << "FAIL: br did not result in Brotli" << std::endl;
    }

    return 0;
}
