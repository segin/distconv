#ifndef CPR_HTTP_CLIENT_H
#define CPR_HTTP_CLIENT_H

#include "../interfaces/http_client_interface.h"
#include <memory>

namespace transcoding_engine {

class CprHttpClient : public IHttpClient {
public:
    CprHttpClient();
    ~CprHttpClient() override;
    
    HttpResponse get(const std::string& url, 
                    const std::map<std::string, std::string>& headers = {}) override;
    
    HttpResponse post(const std::string& url, 
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) override;
    
    HttpResponse download_file(const std::string& url, 
                              const std::string& output_path,
                              const std::map<std::string, std::string>& headers = {}) override;
    
    HttpResponse upload_file(const std::string& url,
                            const std::string& file_path,
                            const std::map<std::string, std::string>& headers = {}) override;
    
    void set_ssl_options(const std::string& ca_cert_path, bool verify_ssl = true) override;
    void set_timeout(int timeout_seconds) override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace transcoding_engine

#endif // CPR_HTTP_CLIENT_H