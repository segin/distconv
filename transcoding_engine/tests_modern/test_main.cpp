#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char **argv) {
    std::cout << "=== DistConv Transcoding Engine Modern Test Suite ===" << std::endl;
    std::cout << "Testing modern C++ architecture with dependency injection" << std::endl;
    std::cout << "=======================================================" << std::endl;
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}