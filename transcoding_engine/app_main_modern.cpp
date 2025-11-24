#include "src/core/transcoding_engine.h"
#include "src/implementations/cpr_http_client.h"
#include "src/implementations/sqlite_database.h"
#include "src/implementations/secure_subprocess.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>

using namespace distconv::TranscodingEngine;

std::atomic<bool> should_stop{false};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    should_stop.store(true);
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  --dispatch-url URL    Dispatch server URL (default: http://localhost:8080)\n"
              << "  --api-key KEY         API key for authentication\n"
              << "  --ca-cert PATH        Path to CA certificate file\n"
              << "  --hostname NAME       Override hostname\n"
              << "  --db-path PATH        Database file path (default: transcoding_jobs.db)\n"
              << "  --engine-id ID        Override engine ID (generated if not specified)\n"
              << "  --storage-gb GB       Storage capacity in GB (default: 500.0)\n"
              << "  --no-streaming        Disable streaming support\n"
              << "  --test-mode           Enable test mode (no background threads)\n"
              << "  --help                Show this help message\n";
}

EngineConfig parse_arguments(int argc, char* argv[]) {
    EngineConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "--dispatch-url" && i + 1 < argc) {
            config.dispatch_server_url = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            config.api_key = argv[++i];
        } else if (arg == "--ca-cert" && i + 1 < argc) {
            config.ca_cert_path = argv[++i];
        } else if (arg == "--hostname" && i + 1 < argc) {
            config.hostname = argv[++i];
        } else if (arg == "--db-path" && i + 1 < argc) {
            config.database_path = argv[++i];
        } else if (arg == "--engine-id" && i + 1 < argc) {
            config.engine_id = argv[++i];
        } else if (arg == "--storage-gb" && i + 1 < argc) {
            try {
                config.storage_capacity_gb = std::stod(argv[++i]);
            } catch (const std::exception& e) {
                std::cerr << "Invalid storage capacity: " << argv[i] << std::endl;
                exit(1);
            }
        } else if (arg == "--no-streaming") {
            config.streaming_support = false;
        } else if (arg == "--test-mode") {
            config.test_mode = true;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Modern Transcoding Engine Starting ===" << std::endl;
    
    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Parse command line arguments
        auto config = parse_arguments(argc, argv);
        
        // Create dependencies with dependency injection
        auto http_client = std::make_unique<CprHttpClient>();
        auto database = std::make_unique<SqliteDatabase>();
        auto subprocess_runner = std::make_unique<SecureSubprocess>();
        
        // Create the transcoding engine
        TranscodingEngine engine(
            std::move(http_client),
            std::move(database),
            std::move(subprocess_runner)
        );
        
        // Initialize the engine
        if (!engine.initialize(config)) {
            std::cerr << "Failed to initialize transcoding engine" << std::endl;
            return 1;
        }
        
        // Register with dispatcher
        if (!engine.register_with_dispatcher()) {
            std::cerr << "Failed to register with dispatcher" << std::endl;
            return 1;
        }
        
        // Start the engine
        if (!engine.start()) {
            std::cerr << "Failed to start transcoding engine" << std::endl;
            return 1;
        }
        
        std::cout << "Transcoding Engine running. Press Ctrl+C to stop." << std::endl;
        
        // Main loop
        while (!should_stop.load() && engine.is_running()) {
            if (config.test_mode) {
                // In test mode, process one job and exit
                auto job = engine.get_job_from_dispatcher();
                if (job.has_value()) {
                    engine.process_job(job.value());
                }
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "Stopping transcoding engine..." << std::endl;
        engine.stop();
        
        std::cout << "Transcoding Engine stopped successfully." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
