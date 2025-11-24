#include <iostream>
#include "submission_client_core.h"

int main(int argc, char* argv[]) {
    std::cout << "Submission Client Application Starting..." << std::endl;
    return distconv::SubmissionsClient::run_submission_client(argc, argv);
}
