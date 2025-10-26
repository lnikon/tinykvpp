#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <db/db.h>

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    // Create test directories
    std::filesystem::create_directories("./var/tkvpp/");

    int result = RUN_ALL_TESTS();

    // Cleanup
    std::filesystem::remove_all("./var/tkvpp/");

    return result;
}
