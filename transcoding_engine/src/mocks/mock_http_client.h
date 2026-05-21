#ifndef MOCK_HTTP_CLIENT_H
#define MOCK_HTTP_CLIENT_H

#include "../interfaces/http_client_interface.h"
#include <queue>
#include <map>

namespace distconv {
namespace TranscodingEngine {

class MockHttpClient : public IHttpClient {
public:
    MockHttpClient() = default;
    ~MockHttpClient() override = default;
    
    // IHttpClient interface
    distconv::TranscodingEngine::HttpResponse get(const std::string& url,
                    const std::map<std::string, std::string>& headers = {}) override;
    
    distconv::TranscodingEngine::HttpResponse post(const std::string& url,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) override;
    
    distconv::TranscodingEngine::HttpResponse download_file(const std::string& url,
                              const std::string& output_path,
                              const std::map<std::string, std::string>& headers = {}) override;
    
    distconv::TranscodingEngine::HttpResponse upload_file(const std::string& url,
                            const std::string& file_path,
                            const std::map<std::string, std::string>& headers = {}) override;
    
    void set_ssl_options(const std::string& ca_cert_path, bool verify_ssl = true) override;
    void set_timeout(int timeout_seconds) override;
    
    // Mock-specific methods
    void set_response_for_url(const std::string& url, const distconv::TranscodingEngine::HttpResponse& response);
    void set_default_response(const distconv::TranscodingEngine::HttpResponse& response);
    void add_response_queue(const std::string& url, const std::queue<distconv::TranscodingEngine::HttpResponse>& responses);
    void clear_responses();
    
    // Call tracking
    struct CallInfo {
        std::string method;
        std::string url;
        std::string body;
        std::map<std::string, std::string> headers;
    };
    
    const std::vector<CallInfo>& get_call_history() const { return call_history_; }
    size_t get_call_count() const { return call_history_.size(); }
    void clear_call_history() { call_history_.clear(); }
    
    // State inspection
    bool was_url_called(const std::string& url) const;
    bool was_method_called(const std::string& method) const;
    CallInfo get_last_call() const;
    
private:
    std::map<std::string, distconv::TranscodingEngine::HttpResponse> url_responses_;
    std::map<std::string, std::queue<distconv::TranscodingEngine::HttpResponse>> url_response_queues_;
    distconv::TranscodingEngine::HttpResponse default_response_{200, "", {}, true, ""};
    std::vector<CallInfo> call_history_;
    
    distconv::TranscodingEngine::HttpResponse get_response_for_url(const std::string& url);
    void record_call(const std::string& method, const std::string& url, 
                    const std::string& body, const std::map<std::string, std::string>& headers);
};

} // namespace TranscodingEngine
} // namespace distconv

#endif // MOCK_HTTP_CLIENT_H
