#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "dispatch_server_core.h"
#include "repositories.h"

int main(int argc, char* argv[]) {
    std::cout << "Modern Dispatch Server Application Starting..." << std::endl;
    
    // Parse command line arguments
    std::string api_key = "";
    std::string database_path = "dispatch_server.db";
    int port = 8080;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--api-key" && i + 1 < argc) {
            api_key = argv[++i];
        } else if (arg == "--database" && i + 1 < argc) {
            database_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --api-key KEY     API key for authentication" << std::endl;
            std::cout << "  --database PATH   SQLite database path (default: dispatch_server.db)" << std::endl;
            std::cout << "  --port PORT       Server port (default: 8080)" << std::endl;
            std::cout << "  --help            Show this help message" << std::endl;
            return 0;
        }
    }
    
    if (api_key.empty()) {
        std::cerr << "Error: API key is required. Use --api-key to specify." << std::endl;
        return 1;
    }
    
    try {
        // Create repositories with dependency injection
        auto job_repo = std::make_unique<SqliteJobRepository>(database_path);
        auto engine_repo = std::make_unique<SqliteEngineRepository>(database_path);
        
        // Create server with injected dependencies
        DispatchServer server(std::move(job_repo), std::move(engine_repo));
        server.set_api_key(api_key);
        
        std::cout << "Starting server on port " << port << " with database: " << database_path << std::endl;
        std::cout << "API key authentication enabled" << std::endl;
        
        // Set up graceful shutdown
        std::atomic<bool> shutdown_requested{false};
        
        // Start server in a separate thread
        std::thread server_thread([&server, port, &shutdown_requested]() {
            server.start(port, false);
            
            // Keep server running until shutdown requested
            while (!shutdown_requested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            server.stop();
        });
        
        std::cout << "Server started successfully. Press Ctrl+C to stop." << std::endl;
        
        // Wait for shutdown signal (in a real app, you'd handle SIGINT/SIGTERM)
        std::string input;
        std::getline(std::cin, input);
        
        std::cout << "Shutting down server..." << std::endl;
        shutdown_requested.store(true);
        
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
        std::cout << "Server stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}