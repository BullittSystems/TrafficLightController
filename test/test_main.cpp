/*
 * test_main.cpp
 *
 * GoogleTest entry point for the native unit-test suite.
 */
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
