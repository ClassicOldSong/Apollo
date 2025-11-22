/**
 * @file tests/unit/test_auto_bitrate.cpp
 * @brief Unit tests for AutoBitrateController.
 */
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "src/auto_bitrate.h"

using namespace auto_bitrate;

class AutoBitrateControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Set up test fixtures if needed
  }

  void TearDown() override {
    // Clean up test fixtures if needed
  }
};

// Test basic initialization
TEST_F(AutoBitrateControllerTest, Initialization) {
  AutoBitrateController controller(20000, 500, 150000);
  EXPECT_EQ(controller.getCurrentBitrate(), 20000);
}

// Test poor network condition - immediate decrease
TEST_F(AutoBitrateControllerTest, PoorNetworkDecrease) {
  AutoBitrateController controller(20000, 500, 150000);

  // Simulate poor network (>5% loss)
  controller.updateNetworkMetrics(10.0f, 2000);
  
  // Wait for adjustment interval
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  
  auto newBitrate = controller.getAdjustedBitrate();
  ASSERT_TRUE(newBitrate.has_value());
  EXPECT_EQ(newBitrate.value(), 10000);  // Should be halved
}

// Test good network condition - increase after stability
TEST_F(AutoBitrateControllerTest, GoodNetworkIncrease) {
  AutoBitrateController controller(10000, 500, 150000);

  // Simulate consecutive good intervals (<1% loss)
  for (int i = 0; i < 3; i++) {
    controller.updateNetworkMetrics(0.5f, 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    controller.getAdjustedBitrate();  // Check but may not adjust yet
  }

  // After stability window, should increase
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  auto newBitrate = controller.getAdjustedBitrate();
  ASSERT_TRUE(newBitrate.has_value());
  EXPECT_EQ(newBitrate.value(), 20000);  // Should be doubled
}

// Test stable network condition - no change
TEST_F(AutoBitrateControllerTest, StableNetworkNoChange) {
  AutoBitrateController controller(20000, 500, 150000);

  // Simulate stable network (1-5% loss)
  controller.updateNetworkMetrics(3.0f, 2000);
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  
  auto newBitrate = controller.getAdjustedBitrate();
  EXPECT_FALSE(newBitrate.has_value());  // Should not adjust
}

// Test minimum bitrate clamping
TEST_F(AutoBitrateControllerTest, MinimumBitrateClamping) {
  AutoBitrateController controller(1000, 500, 150000);

  // Simulate very poor network
  controller.updateNetworkMetrics(20.0f, 2000);
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  
  auto newBitrate = controller.getAdjustedBitrate();
  ASSERT_TRUE(newBitrate.has_value());
  EXPECT_EQ(newBitrate.value(), 500);  // Should clamp to minimum
}

// Test maximum bitrate clamping
TEST_F(AutoBitrateControllerTest, MaximumBitrateClamping) {
  AutoBitrateController controller(100000, 500, 150000);

  // Simulate consecutive good intervals
  for (int i = 0; i < 3; i++) {
    controller.updateNetworkMetrics(0.5f, 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    controller.getAdjustedBitrate();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  auto newBitrate = controller.getAdjustedBitrate();
  ASSERT_TRUE(newBitrate.has_value());
  EXPECT_EQ(newBitrate.value(), 150000);  // Should clamp to maximum
}

// Test reset functionality
TEST_F(AutoBitrateControllerTest, Reset) {
  AutoBitrateController controller(20000, 500, 150000);

  // Change bitrate
  controller.updateNetworkMetrics(10.0f, 2000);
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  controller.getAdjustedBitrate();  // Should decrease

  // Reset to new base
  controller.reset(25000);
  EXPECT_EQ(controller.getCurrentBitrate(), 25000);
}

// Test rapid oscillation prevention
TEST_F(AutoBitrateControllerTest, OscillationPrevention) {
  AutoBitrateController controller(20000, 500, 150000);

  // First decrease
  controller.updateNetworkMetrics(10.0f, 2000);
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  auto bitrate1 = controller.getAdjustedBitrate();
  ASSERT_TRUE(bitrate1.has_value());

  // Immediately try to increase (should be prevented by timing)
  controller.updateNetworkMetrics(0.5f, 2000);
  auto bitrate2 = controller.getAdjustedBitrate();
  EXPECT_FALSE(bitrate2.has_value());  // Should not adjust immediately
}


