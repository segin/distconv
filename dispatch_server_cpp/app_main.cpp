#include <iostream>
#include "dispatch_server_core.h" // Include the core logic

int main(int argc, char* argv[]) {
    std::cout << "Dispatch Server Application Starting..." << std::endl;
    distconv::DispatchServer::DispatchServer server;
    // Pass the address of the server object to run_dispatch_server
    distconv::DispatchServer::run_dispatch_server(argc, argv, &server);
    // In a real application, you would likely have a loop here
    // to keep the main thread alive while the server runs in its own thread.
    // For this example, we'll just let it run for a bit and then stop.
    std::this_thread::sleep_for(std::chrono::seconds(10)); // Run for 10 seconds
    server.stop();
    return 0;
}
