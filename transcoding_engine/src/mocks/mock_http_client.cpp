#include "mock_http_client.h"
#include <fstream>
#include <filesystem>

namespace transcoding_engine {

HttpResponse MockHttpClient::get(const std::string& url, 
                                const std::map<std::string, std::string>& headers) {
    record_call("GET", url, "", headers);
    return get_response_for_url(url);
}

HttpResponse MockHttpClient::post(const std::string& url, 
                                 const std::string& body,
                                 const std::map<std::string, std::string>& headers) {
    record_call("POST", url, body, headers);
    return get_response_for_url(url);
}

HttpResponse MockHttpClient::download_file(const std::string& url, 
                                          const std::string& output_path,
                                          const std::map<std::string, std::string>& headers) {
    record_call("DOWNLOAD", url, output_path, headers);
    auto response = get_response_for_url(url);
    
    if (response.success) {
        // Create a mock file for successful downloads
        std::ofstream file(output_path);
        file << "mock downloaded content for " << url;
        file.close();
    }
    
    return response;
}

HttpResponse MockHttpClient::upload_file(const std::string& url,
                                        const std::string& file_path,
                                        const std::map<std::string, std::string>& headers) {
    record_call("UPLOAD", url, file_path, headers);
    
    // Check if file exists (for realistic behavior)
    if (!std::filesystem::exists(file_path)) {
        return {0, "", {}, false, "File not found: " + file_path};
    }
    
    return get_response_for_url(url);
}

void MockHttpClient::set_ssl_options(const std::string& ca_cert_path, bool verify_ssl) {
    // Mock implementation - just record the settings
    record_call("SET_SSL", ca_cert_path, verify_ssl ? "true" : "false", {});
}

void MockHttpClient::set_timeout(int timeout_seconds) {
    // Mock implementation - just record the setting
    record_call("SET_TIMEOUT", "", std::to_string(timeout_seconds), {});
}

void MockHttpClient::set_response_for_url(const std::string& url, const HttpResponse& response) {
    url_responses_[url] = response;
}

void MockHttpClient::set_default_response(const HttpResponse& response) {
    default_response_ = response;
}

void MockHttpClient::add_response_queue(const std::string& url, const std::queue<HttpResponse>& responses) {
    url_response_queues_[url] = responses;
}

void MockHttpClient::clear_responses() {
    url_responses_.clear();
    url_response_queues_.clear();
}

bool MockHttpClient::was_url_called(const std::string& url) const {
    for (const auto& call : call_history_) {
        if (call.url == url) {
            return true;
        }
    }
    return false;
}

bool MockHttpClient::was_method_called(const std::string& method) const {
    for (const auto& call : call_history_) {
        if (call.method == method) {
            return true;
        }
    }
    return false;
}

MockHttpClient::CallInfo MockHttpClient::get_last_call() const {
    if (call_history_.empty()) {
        return {"", "", "", {}};
    }
    return call_history_.back();
}

HttpResponse MockHttpClient::get_response_for_url(const std::string& url) {
    // Check for queued responses first
    auto queue_it = url_response_queues_.find(url);
    if (queue_it != url_response_queues_.end() && !queue_it->second.empty()) {
        auto response = queue_it->second.front();
        queue_it->second.pop();
        return response;
    }
    
    // Check for specific URL response
    auto it = url_responses_.find(url);
    if (it != url_responses_.end()) {
        return it->second;
    }
    
    // Return default response
    return default_response_;
}

void MockHttpClient::record_call(const std::string& method, const std::string& url, 
                                 const std::string& body, const std::map<std::string, std::string>& headers) {
    call_history_.push_back({method, url, body, headers});
}

} // namespace transcoding_engine