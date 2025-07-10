#include "cpr_http_client.h"
#include <cpr/cpr.h>
#include <fstream>
#include <iostream>

namespace transcoding_engine {

class CprHttpClient::Impl {
public:
    std::string ca_cert_path_;
    bool ssl_verify_ = true;
    int timeout_seconds_ = 30;
    
    cpr::Header create_headers(const std::map<std::string, std::string>& headers) {
        cpr::Header cpr_headers;
        for (const auto& [key, value] : headers) {
            cpr_headers[key] = value;
        }
        return cpr_headers;
    }
    
    cpr::SslOptions create_ssl_options() {
        if (!ssl_verify_) {
            return cpr::SslOptions{cpr::ssl::VerifyPeer{false}, cpr::ssl::VerifyHost{false}};
        }
        
        if (!ca_cert_path_.empty()) {
            return cpr::SslOptions{cpr::ssl::CaInfo{ca_cert_path_}};
        }
        
        return cpr::SslOptions{};
    }
    
    HttpResponse convert_response(const cpr::Response& response) {
        HttpResponse http_response;
        http_response.status_code = static_cast<int>(response.status_code);
        http_response.body = response.text;
        http_response.success = (response.status_code >= 200 && response.status_code < 300);
        
        if (response.error.code != cpr::ErrorCode::OK) {
            http_response.success = false;
            http_response.error_message = response.error.message;
        }
        
        // Convert headers
        for (const auto& [key, value] : response.header) {
            http_response.headers[key] = value;
        }
        
        return http_response;
    }
};

CprHttpClient::CprHttpClient() : pimpl_(std::make_unique<Impl>()) {}

CprHttpClient::~CprHttpClient() = default;

HttpResponse CprHttpClient::get(const std::string& url, 
                               const std::map<std::string, std::string>& headers) {
    try {
        auto response = cpr::Get(
            cpr::Url{url},
            pimpl_->create_headers(headers),
            pimpl_->create_ssl_options(),
            cpr::Timeout{pimpl_->timeout_seconds_ * 1000}
        );
        
        return pimpl_->convert_response(response);
    } catch (const std::exception& e) {
        return {0, "", {}, false, std::string("Exception in GET request: ") + e.what()};
    }
}

HttpResponse CprHttpClient::post(const std::string& url, 
                                const std::string& body,
                                const std::map<std::string, std::string>& headers) {
    try {
        auto response = cpr::Post(
            cpr::Url{url},
            cpr::Body{body},
            pimpl_->create_headers(headers),
            pimpl_->create_ssl_options(),
            cpr::Timeout{pimpl_->timeout_seconds_ * 1000}
        );
        
        return pimpl_->convert_response(response);
    } catch (const std::exception& e) {
        return {0, "", {}, false, std::string("Exception in POST request: ") + e.what()};
    }
}

HttpResponse CprHttpClient::download_file(const std::string& url, 
                                         const std::string& output_path,
                                         const std::map<std::string, std::string>& headers) {
    try {
        std::ofstream file(output_path, std::ios::binary);
        if (!file.is_open()) {
            return {0, "", {}, false, "Failed to open output file: " + output_path};
        }
        
        auto response = cpr::Download(
            file,
            cpr::Url{url},
            pimpl_->create_headers(headers),
            pimpl_->create_ssl_options(),
            cpr::Timeout{pimpl_->timeout_seconds_ * 1000}
        );
        
        file.close();
        
        auto http_response = pimpl_->convert_response(response);
        if (!http_response.success) {
            // Clean up failed download
            std::remove(output_path.c_str());
        }
        
        return http_response;
    } catch (const std::exception& e) {
        std::remove(output_path.c_str());
        return {0, "", {}, false, std::string("Exception in download: ") + e.what()};
    }
}

HttpResponse CprHttpClient::upload_file(const std::string& url,
                                       const std::string& file_path,
                                       const std::map<std::string, std::string>& headers) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return {0, "", {}, false, "Failed to open input file: " + file_path};
        }
        
        // Read file content
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        auto response = cpr::Post(
            cpr::Url{url},
            cpr::Body{content},
            pimpl_->create_headers(headers),
            pimpl_->create_ssl_options(),
            cpr::Timeout{pimpl_->timeout_seconds_ * 1000}
        );
        
        return pimpl_->convert_response(response);
    } catch (const std::exception& e) {
        return {0, "", {}, false, std::string("Exception in upload: ") + e.what()};
    }
}

void CprHttpClient::set_ssl_options(const std::string& ca_cert_path, bool verify_ssl) {
    pimpl_->ca_cert_path_ = ca_cert_path;
    pimpl_->ssl_verify_ = verify_ssl;
}

void CprHttpClient::set_timeout(int timeout_seconds) {
    pimpl_->timeout_seconds_ = timeout_seconds;
}

} // namespace transcoding_engine