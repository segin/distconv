#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <future>
#include <stdexcept>
#include <uuid/uuid.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include "request_handlers.h"
#include "job_handlers.h"
#include "engine_handlers.h"
#include "job_action_handlers.h"
#include "assignment_handler.h"
#include "job_update_handler.h"
#include "storage_pool_handler.h"
#include "api_middleware.h"
#include "enhanced_endpoints.h"

namespace distconv {
namespace DispatchServer {

using namespace Constants;

// Global flag for mocking
bool mock_save_state_enabled = false;
std::atomic<int> save_state_call_count{0};

// Utility function to generate UUID for job IDs
std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

// Job and Engine struct implementations
nlohmann::json Job::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["status"] = status;
    j["source_url"] = source_url;
    j["output_url"] = output_url;
    j["assigned_engine"] = assigned_engine;
    j["codec"] = codec;
    j["job_size"] = job_size;
    j["max_retries"] = max_retries;
    j["retries"] = retries;
    j["priority"] = priority;
    j["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(created_at.time_since_epoch()).count();
    j["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(updated_at.time_since_epoch()).count();
    return j;
}

Job Job::from_json(const nlohmann::json& j) {
    Job job;
    job.id = j.value("id", "");
    job.status = j.value("status", "pending");
    job.source_url = j.value("source_url", "");
    job.output_url = j.value("output_url", "");
    job.assigned_engine = j.value("assigned_engine", "");
    job.codec = j.value("codec", "");
    job.job_size = j.value("job_size", 0.0);
    job.max_retries = j.value("max_retries", 3);
    job.retries = j.value("retries", 0);
    job.priority = j.value("priority", 0);
    
    if (j.contains("created_at")) {
        job.created_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["created_at"]}};
    } else {
        job.created_at = std::chrono::system_clock::now();
    }
    
    if (j.contains("updated_at")) {
        job.updated_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["updated_at"]}};
    } else {
        job.updated_at = std::chrono::system_clock::now();
    }
    
    return job;
}

nlohmann::json Engine::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["hostname"] = hostname;
    j["status"] = status;
    j["benchmark_time"] = benchmark_time;
    j["can_stream"] = can_stream;
    j["storage_capacity_gb"] = storage_capacity_gb;
    j["current_job_id"] = current_job_id;
    j["last_heartbeat"] = std::chrono::duration_cast<std::chrono::milliseconds>(last_heartbeat.time_since_epoch()).count();
    return j;
}

Engine Engine::from_json(const nlohmann::json& j) {
    Engine engine;
    engine.id = j.value("id", "");
    engine.hostname = j.value("hostname", "");
    engine.status = j.value("status", "idle");
    engine.benchmark_time = j.value("benchmark_time", 0.0);
    engine.can_stream = j.value("can_stream", false);
    engine.storage_capacity_gb = j.value("storage_capacity_gb", 0);
    engine.current_job_id = j.value("current_job_id", "");
    
    if (j.contains("last_heartbeat")) {
        engine.last_heartbeat = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["last_heartbeat"]}};
    } else {
        engine.last_heartbeat = std::chrono::system_clock::now();
    }
    
    return engine;
}

// DispatchServer Implementation

DispatchServer::DispatchServer(const std::string& api_key) 
    : job_repo_(std::make_shared<SqliteJobRepository>("dispatch_jobs.db")),
      engine_repo_(std::make_shared<SqliteEngineRepository>("dispatch_engines.db")),
      api_key_(api_key) {
    
    // Initialize Tdarr client with default URL or from environment
    const char* tdarr_url_env = std::getenv("TDARR_URL");
    std::string tdarr_url = tdarr_url_env ? tdarr_url_env : "http://localhost:8265";
    tdarr_client_ = std::make_unique<Tdarr::TdarrClient>(tdarr_url);

    if (!api_key_.empty()) {
        setup_endpoints();
    }
}

DispatchServer::DispatchServer(std::shared_ptr<IJobRepository> job_repo, 
                               std::shared_ptr<IEngineRepository> engine_repo,
                               const std::string& api_key) 
    : job_repo_(job_repo), 
      engine_repo_(engine_repo),
      api_key_(api_key) {
    
    // Initialize Tdarr client with default URL or from environment
    const char* tdarr_url_env = std::getenv("TDARR_URL");
    std::string tdarr_url = tdarr_url_env ? tdarr_url_env : "http://localhost:8265";
    tdarr_client_ = std::make_unique<Tdarr::TdarrClient>(tdarr_url);

    if (!api_key_.empty()) {
        setup_endpoints();
    }
}

DispatchServer::DispatchServer(std::shared_ptr<IJobRepository> job_repo,
                               std::shared_ptr<IEngineRepository> engine_repo,
                               std::unique_ptr<MessageQueueFactory> mq_factory,
                               const std::string& api_key)
    : job_repo_(job_repo),
      engine_repo_(engine_repo),
      mq_factory_(std::move(mq_factory)),
      api_key_(api_key) {

    if (mq_factory_) {
        job_publisher_ = std::make_shared<JobPublisher>(mq_factory_->createProducer());
        status_subscriber_ = std::make_shared<StatusSubscriber>(mq_factory_->createConsumer("dispatch-server-group"));
        status_subscriber_->subscribeToStatusUpdates([this](const std::string& message_payload) {
            try {
                auto j = nlohmann::json::parse(message_payload);
                if (j.contains("job_id") && j.contains("status")) {
                    std::string job_id = j["job_id"];
                    std::string status = j["status"];

                    if (this->job_repo_->job_exists(job_id)) {
                        auto job = this->job_repo_->get_job(job_id);
                        job["status"] = status;
                        if (j.contains("output_url")) job["output_url"] = j["output_url"];
                        if (j.contains("error_message")) job["error_message"] = j["error_message"];
                        this->job_repo_->save_job(job_id, job);
                    }
                }
            } catch (...) {}
        });
    }

    if (!api_key_.empty()) {
        setup_endpoints();
    }
}

DispatchServer::DispatchServer() 
    : job_repo_(std::make_shared<SqliteJobRepository>("dispatch_jobs.db")),
      engine_repo_(std::make_shared<SqliteEngineRepository>("dispatch_engines.db")) {
    
    // Initialize Tdarr client with default URL or from environment
    const char* tdarr_url_env = std::getenv("TDARR_URL");
    std::string tdarr_url = tdarr_url_env ? tdarr_url_env : "http://localhost:8265";
    tdarr_client_ = std::make_unique<Tdarr::TdarrClient>(tdarr_url);
}

DispatchServer::~DispatchServer() {
    stop();
}

void DispatchServer::set_api_key(const std::string& key) {
    api_key_ = key;
    setup_endpoints();
}

void DispatchServer::start(int port, bool block) {
    shutdown_requested_.store(false);
    background_worker_thread_ = std::thread(&DispatchServer::background_worker, this);

    int bound_port = -1;
    if (port == 0) {
        bound_port = svr.bind_to_any_port("0.0.0.0");
    } else {
        if (svr.bind_to_port("0.0.0.0", port)) {
            bound_port = port;
        }
    }

    if (bound_port == -1) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return;
    }

    bound_port_ = bound_port;
    
    if (block) {
        svr.listen_after_bind();
    } else {
        server_thread = std::thread([this]() {
            this->svr.listen_after_bind();
        });
    }
}

void DispatchServer::stop() {
    shutdown_requested_.store(true);
    shutdown_cv_.notify_all();
    if (background_worker_thread_.joinable()) {
        background_worker_thread_.join();
    }
    
    if (svr.is_running()) {
        svr.stop();
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }
    std::cout << "DispatchServer stopped. State managed by repositories." << std::endl;
}

httplib::Server* DispatchServer::getServer() {
    return &svr;
}

void DispatchServer::background_worker() {
    while (!shutdown_requested_.load()) {
        try {
            cleanup_stale_engines();
            handle_job_timeouts();
            requeue_failed_jobs();
            expire_pending_jobs();
            
            std::unique_lock<std::mutex> lock(shutdown_mutex_);
            shutdown_cv_.wait_for(lock, BACKGROUND_WORKER_INTERVAL, [this] {
                return shutdown_requested_.load();
            });
        } catch (const std::exception& e) {
            std::cerr << "Background worker error: " << e.what() << std::endl;
        }
    }
}

void DispatchServer::cleanup_stale_engines() {
    auto engines = engine_repo_->get_all_engines();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int64_t timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ENGINE_HEARTBEAT_TIMEOUT).count();

    for (const auto& engine : engines) {
        if (engine.contains("last_heartbeat")) {
            int64_t last_heartbeat = engine["last_heartbeat"];
            int64_t diff_ms = now_ms - last_heartbeat;
            
            if (diff_ms > timeout_ms) {
                std::string engine_id = engine["engine_id"];
                std::cout << "Removing stale engine: " << engine_id << std::endl;
                
                if (engine.contains("current_job_id") && !engine["current_job_id"].is_null()) {
                    std::string job_id = engine["current_job_id"];
                    if (job_repo_->job_exists(job_id)) {
                        nlohmann::json job = job_repo_->get_job(job_id);
                        if (job["status"] == "assigned" || job["status"] == "processing") {
                            job["status"] = "pending";
                            job["assigned_engine"] = nullptr;
                            job["retries"] = job.value("retries", 0) + 1;
                            job_repo_->save_job(job_id, job);
                        }
                    }
                }
                engine_repo_->remove_engine(engine_id);
            }
        }
    }
}

void DispatchServer::handle_job_timeouts() {
    auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(JOB_TIMEOUT).count();
    auto jobs = job_repo_->get_jobs_to_timeout(timeout_ms);
    
    for (auto& job : jobs) {
        std::string job_id = job["job_id"];
        std::cout << "Job " << job_id << " timed out, marking as failed" << std::endl;

        int retries = job.value("retries", 0);
        int max_retries = job.value("max_retries", 3);

        if (retries < max_retries) {
            job["status"] = "failed_retry";
            job["retries"] = retries + 1;
            job["retry_after"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch() + std::chrono::seconds(30)).count();
        } else {
            job["status"] = "failed_permanently";
            job["error_message"] = "Job timed out and exceeded max retries";
        }

        if (job.contains("assigned_engine") && !job["assigned_engine"].is_null()) {
            std::string engine_id = job["assigned_engine"];
            nlohmann::json engine = engine_repo_->get_engine(engine_id);
            if (!engine.is_null()) {
                engine["status"] = "idle";
                engine["current_job_id"] = "";
                engine_repo_->save_engine(engine_id, engine);
            }
        }
        job_repo_->save_job(job_id, job);
    }
}

void DispatchServer::requeue_failed_jobs() {
    auto jobs = job_repo_->get_jobs_by_status("failed_retry");
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (auto& job : jobs) {
        int64_t retry_after = job.value("retry_after", 0LL);
        if (now_ms >= retry_after) {
            job["status"] = "pending";
            job_repo_->save_job(job["job_id"], job);
        }
    }
}

void DispatchServer::expire_pending_jobs() {
    int64_t timeout_seconds = 24 * 3600; 
    auto stale_job_ids = job_repo_->get_stale_pending_jobs(timeout_seconds);
    
    for (const auto& job_id : stale_job_ids) {
        nlohmann::json job = job_repo_->get_job(job_id);
        job["status"] = "expired";
        job["error_message"] = "Job expired after being pending for too long";
        job_repo_->save_job(job_id, job);
    }
}

// Endpoint Setup

void DispatchServer::setup_endpoints() {
    setup_system_endpoints();
    setup_job_endpoints();
    setup_engine_endpoints();
    setup_storage_endpoints();
    setup_tdarr_endpoints();
    
    // Wire up enhanced endpoints
    setup_enhanced_job_endpoints(svr, api_key_, job_repo_, engine_repo_);
    setup_enhanced_system_endpoints(svr, api_key_, job_repo_, engine_repo_);
}

void DispatchServer::setup_system_endpoints() {
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json health;
        health["status"] = "healthy";
        res.set_content(health.dump(), "application/json");
    });
}

void DispatchServer::setup_job_endpoints() {
    auto auth = std::make_shared<AuthMiddleware>(api_key_);
    
    auto submit_handler = std::make_shared<JobSubmissionHandler>(auth, job_repo_);
    svr.Post("/jobs/", [submit_handler](const httplib::Request& req, httplib::Response& res) {
        submit_handler->handle(req, res);
    });

    auto status_handler = std::make_shared<JobStatusHandler>(auth, job_repo_);
    svr.Get(R"(/jobs/([a-fA-F0-9\-]{36}))", [status_handler](const httplib::Request& req, httplib::Response& res) {
        status_handler->handle(req, res);
    });

    auto list_handler = std::make_shared<JobListHandler>(auth, job_repo_);
    svr.Get("/jobs/", [list_handler](const httplib::Request& req, httplib::Response& res) {
        list_handler->handle(req, res);
    });

    auto complete_handler = std::make_shared<JobCompletionHandler>(auth, job_repo_, engine_repo_);
    svr.Post(R"(/jobs/([a-fA-F0-9\-]{36})/complete)", [complete_handler](const httplib::Request& req, httplib::Response& res) {
        complete_handler->handle(req, res);
    });

    auto fail_handler = std::make_shared<JobFailureHandler>(auth, job_repo_, engine_repo_);
    svr.Post(R"(/jobs/([a-fA-F0-9\-]{36})/fail)", [fail_handler](const httplib::Request& req, httplib::Response& res) {
        fail_handler->handle(req, res);
    });

    auto retry_handler = std::make_shared<JobRetryHandler>(auth, job_repo_);
    svr.Post(R"(/jobs/([a-fA-F0-9\-]{36})/retry)", [retry_handler](const httplib::Request& req, httplib::Response& res) {
        retry_handler->handle(req, res);
    });

    auto cancel_handler = std::make_shared<JobCancelHandler>(auth, job_repo_, engine_repo_);
    svr.Post(R"(/jobs/([a-fA-F0-9\-]{36})/cancel)", [cancel_handler](const httplib::Request& req, httplib::Response& res) {
        cancel_handler->handle(req, res);
    });

    auto update_handler = std::make_shared<JobUpdateHandler>(auth, job_repo_);
    svr.Put(R"(/jobs/([a-fA-F0-9\-]{36}))", [update_handler](const httplib::Request& req, httplib::Response& res) {
        update_handler->handle(req, res);
    });

    auto progress_handler = std::make_shared<JobProgressHandler>(auth, job_repo_);
    svr.Post(R"(/jobs/([a-fA-F0-9\-]{36})/progress)", [progress_handler](const httplib::Request& req, httplib::Response& res) {
        progress_handler->handle(req, res);
    });
}

void DispatchServer::setup_engine_endpoints() {
    auto auth = std::make_shared<AuthMiddleware>(api_key_);

    auto heartbeat_handler = std::make_shared<EngineHeartbeatHandler>(auth, engine_repo_);
    svr.Post("/engines/heartbeat", [heartbeat_handler](const httplib::Request& req, httplib::Response& res) {
        heartbeat_handler->handle(req, res);
    });

    auto list_handler = std::make_shared<EngineListHandler>(auth, engine_repo_);
    svr.Get("/engines/", [list_handler](const httplib::Request& req, httplib::Response& res) {
        list_handler->handle(req, res);
    });

    auto assignment_handler = std::make_shared<JobAssignmentHandler>(auth, job_repo_, engine_repo_);
    svr.Post("/assign_job/", [assignment_handler](const httplib::Request& req, httplib::Response& res) {
        assignment_handler->handle(req, res);
    });
    
    auto benchmark_handler = std::make_shared<EngineBenchmarkHandler>(auth, engine_repo_);
    svr.Post("/engines/benchmark_result", [benchmark_handler](const httplib::Request& req, httplib::Response& res) {
        benchmark_handler->handle(req, res);
    });
}

void DispatchServer::setup_storage_endpoints() {
    auto auth = std::make_shared<AuthMiddleware>(api_key_);
    static auto storage_repo = std::make_shared<InMemoryStorageRepository>();
    
    auto create_handler = std::make_shared<StoragePoolCreateHandler>(auth, storage_repo);
    svr.Post("/storage_pools/", [create_handler](const httplib::Request& req, httplib::Response& res) {
        create_handler->handle(req, res);
    });

    auto list_handler = std::make_shared<StoragePoolListHandler>(auth, storage_repo);
    svr.Get("/storage_pools/", [list_handler](const httplib::Request& req, httplib::Response& res) {
        list_handler->handle(req, res);
    });
}

void DispatchServer::setup_tdarr_endpoints() {
    std::cout << "Registering Tdarr integration endpoints..." << std::endl;
    auto auth = std::make_shared<AuthMiddleware>(api_key_);

    svr.Get("/tdarr/status", [this, auth](const httplib::Request& req, httplib::Response& res) {
        if (!auth->authenticate(req, res)) return;
        
        nlohmann::json status;
        if (tdarr_client_ && tdarr_client_->check_health()) {
            status["tdarr_server"] = "online";
        } else {
            status["tdarr_server"] = "offline";
        }
        res.set_content(status.dump(), "application/json");
    });

    svr.Post("/tdarr/submit", [this, auth](const httplib::Request& req, httplib::Response& res) {
        if (!auth->authenticate(req, res)) return;

        try {
            auto input = nlohmann::json::parse(req.body);
            std::string tdarr_job_id;
            if (tdarr_client_ && tdarr_client_->submit_job(input, tdarr_job_id)) {
                nlohmann::json response;
                response["status"] = "submitted_to_tdarr";
                response["tdarr_job_id"] = tdarr_job_id;
                res.set_content(response.dump(), "application/json");
            } else {
                res.status = 502; // Bad Gateway
                res.set_content("{\"error\": \"Failed to submit job to Tdarr\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        }
    });
}

// Global functions for backward compatibility

DispatchServer* run_dispatch_server(int argc, char* argv[], DispatchServer* server_instance) {
    std::string api_key = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--api-key" && i + 1 < argc) {
            api_key = argv[++i];
        }
    }

    if (server_instance) {
        server_instance->set_api_key(api_key);
        std::cout << "Dispatch Server listening on port 8080" << std::endl;
        server_instance->start(8080, false);
    }

    return server_instance;
}

DispatchServer* run_dispatch_server(DispatchServer* server_instance) {
    if (server_instance) {
        server_instance->set_api_key("");
        std::cout << "Dispatch Server listening on port 8080" << std::endl;
        server_instance->start(8080, false);
    }
    return server_instance;
}

} // namespace DispatchServer
} // namespace distconv
