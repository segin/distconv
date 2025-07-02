#include <iostream>
#include "dispatch_server_core.h" // Include the core logic

int main(int argc, char* argv[]) {
    std::cout << "Dispatch Server Application Starting..." << std::endl;
    DispatchServer server;
    server.start(8080);
    return 0;
}
