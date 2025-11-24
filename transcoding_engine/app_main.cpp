#include <iostream>
#include "transcoding_engine_core.h"

int main(int argc, char* argv[]) {
    std::cout << "Transcoding Engine Application Starting..." << std::endl;
    return distconv::TranscodingEngine::run_transcoding_engine(argc, argv);
}
