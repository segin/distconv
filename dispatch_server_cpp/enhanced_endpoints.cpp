#include "enhanced_endpoints.h"
#include "dispatch_server_core.h"
#include <regex>
#include <chrono>
#include <iostream>

namespace distconv {
namespace DispatchServer {

// External helper
extern std::string generate_uuid();

namespace EnhancedEndpoints {
    
    // Utility functions for validation
    bool is_valid_url(const std::string& url) {
        if (url.empty()) return false;
        std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)", std::regex_constants::icase);
        return std::regex_match(url, url_regex);
    }
    
    void error_response(httplib::Response& res, const std::string& code, const std::string& message, int status = 400) {
        nlohmann::json error;
        error["error"] = {{"code", code}, {"message", message}};
        res.status = status;
        res.set_content(error.dump(), "application/json");
    }
    
    void success_response(httplib::Response& res, const nlohmann::json& data, int status = 200) {
        res.status = status;
        res.set_content(data.dump(), "application/json");
    }
    
    bool validate_api_key(const httplib::Request& req, httplib::Response& res, const std::string& api_key) {
        if (api_key.empty()) return true;
        std::string provided_key = req.get_header_value("X-API-Key");
        if (provided_key != api_key) {
            error_response(res, "UNAUTHORIZED", "Invalid or missing API key", 401);
            return false;
        }
        return true;
    }
}

void setup_enhanced_job_endpoints(httplib::Server &svr, const std::string& api_key, 
                                 std::shared_ptr<IJobRepository> job_repo,
                                 std::shared_ptr<IEngineRepository> engine_repo) {
    
    // POST /api/v1/jobs - Enhanced job submission
    svr.Post("/api/v1/jobs", [api_key, job_repo](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("source_url") || !request_json["source_url"].is_string()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "source_url required");
                return;
            }
            
            std::string job_id = generate_uuid();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            nlohmann::json job;
            job["job_id"] = job_id;
            job["source_url"] = request_json["source_url"];
            job["target_codec"] = request_json.value("target_codec", "h264");
            job["status"] = "pending";
            job["created_at"] = now_ms;
            job["updated_at"] = now_ms;
            job["priority"] = request_json.value("priority", 0);
            
            job_repo->save_job(job_id, job);
            EnhancedEndpoints::success_response(res, job, 201);
            
        } catch (const std::exception& e) {
            EnhancedEndpoints::error_response(res, "INTERNAL_ERROR", e.what(), 500);
        }
    });

    // GET /api/v1/jobs/{id} - Get job status
    svr.Get(R"(/api/v1/jobs/([a-fA-F0-9\-]{36}))", [api_key, job_repo](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        std::string job_id = req.matches[1];
        nlohmann::json job = job_repo->get_job(job_id);
        if (job.is_null()) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job not found", 404);
        } else {
            EnhancedEndpoints::success_response(res, job);
        }
    });
}

void setup_enhanced_system_endpoints(httplib::Server &svr, const std::string& api_key,
                                    std::shared_ptr<IJobRepository> job_repo,
                                    std::shared_ptr<IEngineRepository> engine_repo) {
    
    // GET /api/v1/status - Enhanced status
    svr.Get("/api/v1/status", [api_key, job_repo, engine_repo](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        nlohmann::json status;
        status["status"] = "healthy";
        auto jobs = job_repo->get_all_jobs();
        auto engines = engine_repo->get_all_engines();
        
        status["jobs_total"] = jobs.size();
        status["engines_total"] = engines.size();
        
        nlohmann::json job_stats;
        for (const auto& j : jobs) {
            std::string s = j.value("status", "unknown");
            job_stats[s] = job_stats.value(s, 0) + 1;
        }
        status["job_statistics"] = job_stats;
        
        EnhancedEndpoints::success_response(res, status);
    });

    // DELETE /api/v1/engines/{id} - Deregister engine
    svr.Delete(R"(/api/v1/engines/([a-zA-Z0-9\-_]+))", [api_key, job_repo, engine_repo](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        std::string engine_id = req.matches[1];
        
        if (!engine_repo->engine_exists(engine_id)) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Engine not found", 404);
            return;
        }

        auto jobs = job_repo->get_jobs_by_engine(engine_id);
        for (auto& job : jobs) {
            job["status"] = "pending";
            job["assigned_engine"] = nullptr;
            job_repo->save_job(job["job_id"], job);
        }

        engine_repo->remove_engine(engine_id);
        nlohmann::json response;
        response["message"] = "Engine deregistered";
        EnhancedEndpoints::success_response(res, response);
    });
}

} // namespace DispatchServer
} // namespace distconv
