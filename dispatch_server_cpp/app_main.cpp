#include <iostream>
#include "dispatch_server_core.h" // Include the core logic

int main(int argc, char* argv[]) {
    std::cout << "Dispatch Server Application Starting..." << std::endl;
    // Call the main server function from the core library
    return run_dispatch_server(argc, argv);
}