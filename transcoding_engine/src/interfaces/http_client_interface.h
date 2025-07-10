#ifndef HTTP_CLIENT_INTERFACE_H
#define HTTP_CLIENT_INTERFACE_H

#include <string>
#include <map>

namespace transcoding_engine {

struct HttpResponse {
    int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success;
    std::string error_message;
};

struct HttpRequest {
    std::string url;
    std::string method;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string ca_cert_path;
    bool ssl_verify = true;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    
    virtual HttpResponse get(const std::string& url, 
                           const std::map<std::string, std::string>& headers = {}) = 0;
    
    virtual HttpResponse post(const std::string& url, 
                            const std::string& body,
                            const std::map<std::string, std::string>& headers = {}) = 0;
    
    virtual HttpResponse download_file(const std::string& url, 
                                     const std::string& output_path,
                                     const std::map<std::string, std::string>& headers = {}) = 0;
    
    virtual HttpResponse upload_file(const std::string& url,
                                   const std::string& file_path,
                                   const std::map<std::string, std::string>& headers = {}) = 0;
    
    virtual void set_ssl_options(const std::string& ca_cert_path, bool verify_ssl = true) = 0;
    virtual void set_timeout(int timeout_seconds) = 0;
};

} // namespace transcoding_engine

#endif // HTTP_CLIENT_INTERFACE_H