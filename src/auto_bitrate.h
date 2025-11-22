/**
 * @file src/auto_bitrate.h
 * @brief Auto bitrate adjustment controller based on network conditions.
 */
#pragma once

#include <chrono>
#include <optional>

namespace auto_bitrate {

  /**
   * @brief Network quality metrics tracked over time.
   */
  struct NetworkMetrics {
    float frameLossPercent = 0.0f;
    int consecutiveGoodIntervals = 0;
    int consecutivePoorIntervals = 0;
    std::chrono::steady_clock::time_point lastAdjustment;
    std::chrono::steady_clock::time_point lastPoorCondition;
  };

  /**
   * @brief Controller for automatic bitrate adjustment based on network conditions.
   *
   * Uses exponential decay/increase algorithm:
   * - Poor network (>5% loss): Immediately halve bitrate
   * - Good network (<1% loss): After stability period, double bitrate
   * - Stable (1-5% loss): Maintain current bitrate
   */
  class AutoBitrateController {
  public:
    /**
     * @brief Construct a new AutoBitrateController.
     * @param initialBitrate Initial bitrate in kbps
     * @param minBitrate Minimum allowed bitrate in kbps (default: 500)
     * @param maxBitrate Maximum allowed bitrate in kbps (default: 150000)
     * @param poorNetworkThreshold Frame loss percentage threshold for poor network (default: 5.0%)
     * @param goodNetworkThreshold Frame loss percentage threshold for good network (default: 1.0%)
     * @param increaseFactor Multiplier for bitrate increase (default: 1.2)
     * @param decreaseFactor Multiplier for bitrate decrease (default: 0.8)
     * @param stabilityWindowMs Time window for stability check in milliseconds (default: 5000)
     * @param minConsecutiveGoodIntervals Minimum consecutive good intervals before increase (default: 3)
     */
    AutoBitrateController(
      int initialBitrate,
      int minBitrate = 500,
      int maxBitrate = 150000,
      float poorNetworkThreshold = 5.0f,
      float goodNetworkThreshold = 1.0f,
      float increaseFactor = 1.2f,
      float decreaseFactor = 0.8f,
      int stabilityWindowMs = 5000,
      int minConsecutiveGoodIntervals = 3
    );

    /**
     * @brief Update network metrics with latest frame loss statistics.
     * @param frameLossPercent Frame loss percentage (0-100)
     * @param timeSinceLastReportMs Time since last report in milliseconds
     */
    void updateNetworkMetrics(float frameLossPercent, int timeSinceLastReportMs);

    /**
     * @brief Get adjusted bitrate if adjustment is needed.
     * @return New bitrate in kbps if adjustment needed, empty optional otherwise
     */
    std::optional<int> getAdjustedBitrate();

    /**
     * @brief Reset controller with new base bitrate.
     * @param newBaseBitrate New base bitrate in kbps
     */
    void reset(int newBaseBitrate);

    /**
     * @brief Get current bitrate.
     * @return Current bitrate in kbps
     */
    int getCurrentBitrate() const {
      return currentBitrateKbps;
    }

  private:
    int currentBitrateKbps;
    int baseBitrateKbps;
    int minBitrateKbps;
    int maxBitrateKbps;

    // Algorithm parameters (configurable)
    float increaseFactor;
    float decreaseFactor;
    int stabilityWindowMs;
    static constexpr int ADJUSTMENT_INTERVAL_MS = 2000;  // Check every 2 seconds
    float poorNetworkThreshold;
    float goodNetworkThreshold;
    int minConsecutiveGoodIntervals;

    NetworkMetrics metrics;
    std::chrono::steady_clock::time_point lastCheckTime;
    bool pendingAdjustment = false;
  };

}  // namespace auto_bitrate


