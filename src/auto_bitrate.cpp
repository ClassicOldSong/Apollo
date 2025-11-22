/**
 * @file src/auto_bitrate.cpp
 * @brief Implementation of auto bitrate adjustment controller.
 */
#include "auto_bitrate.h"

#include "logging.h"

namespace auto_bitrate {

  AutoBitrateController::AutoBitrateController(
      int initialBitrate,
      int minBitrate,
      int maxBitrate,
      float poorNetworkThreshold,
      float goodNetworkThreshold,
      float increaseFactor,
      float decreaseFactor,
      int stabilityWindowMs,
      int minConsecutiveGoodIntervals)
      : currentBitrateKbps(initialBitrate),
        baseBitrateKbps(initialBitrate),
        minBitrateKbps(minBitrate),
        maxBitrateKbps(maxBitrate),
        increaseFactor(increaseFactor),
        decreaseFactor(decreaseFactor),
        stabilityWindowMs(stabilityWindowMs),
        poorNetworkThreshold(poorNetworkThreshold),
        goodNetworkThreshold(goodNetworkThreshold),
        minConsecutiveGoodIntervals(minConsecutiveGoodIntervals),
        lastCheckTime(std::chrono::steady_clock::now()) {
    metrics.lastAdjustment = std::chrono::steady_clock::now();
    metrics.lastPoorCondition = std::chrono::steady_clock::now();
  }

  void AutoBitrateController::updateNetworkMetrics(float frameLossPercent, int timeSinceLastReportMs) {
    // Clamp frame loss percentage to non-negative to handle data corruption or counter issues
    // Negative values would incorrectly trigger good network conditions
    frameLossPercent = std::max(0.0f, frameLossPercent);
    metrics.frameLossPercent = frameLossPercent;

    auto now = std::chrono::steady_clock::now();

    // Update consecutive interval counters
    if (frameLossPercent > poorNetworkThreshold) {
      metrics.consecutivePoorIntervals++;
      metrics.consecutiveGoodIntervals = 0;
      metrics.lastPoorCondition = now;
    } else if (frameLossPercent < goodNetworkThreshold) {
      metrics.consecutiveGoodIntervals++;
      metrics.consecutivePoorIntervals = 0;
    } else {
      // Stable zone: reset counters but don't change bitrate
      metrics.consecutiveGoodIntervals = 0;
      metrics.consecutivePoorIntervals = 0;
    }
  }

  std::optional<int> AutoBitrateController::getAdjustedBitrate() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheckTime).count();

    // Only check for adjustments at regular intervals
    if (timeSinceLastCheck < ADJUSTMENT_INTERVAL_MS) {
      return std::nullopt;
    }

    lastCheckTime = now;

    // Check for poor network conditions - immediate decrease
    if (metrics.frameLossPercent > poorNetworkThreshold) {
      // Check if we've already adjusted recently (avoid rapid oscillations)
      auto timeSinceLastAdjustment = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastAdjustment).count();
      if (timeSinceLastAdjustment < ADJUSTMENT_INTERVAL_MS) {
        return std::nullopt;
      }

      int newBitrate = static_cast<int>(currentBitrateKbps * decreaseFactor);
      newBitrate = std::max(newBitrate, minBitrateKbps);

      if (newBitrate != currentBitrateKbps) {
        BOOST_LOG(info) << "AutoBitrate: Poor network detected (" << metrics.frameLossPercent
                        << "% loss), decreasing bitrate from " << currentBitrateKbps << " to " << newBitrate << " kbps";
        currentBitrateKbps = newBitrate;
        metrics.lastAdjustment = now;
        metrics.consecutiveGoodIntervals = 0;
        metrics.consecutivePoorIntervals = 0;
        return newBitrate;
      }
    }
    // Check for good network conditions - increase after stability period
    else if (metrics.frameLossPercent < goodNetworkThreshold) {
      // Require consecutive good intervals and stability window
      if (metrics.consecutiveGoodIntervals >= minConsecutiveGoodIntervals) {
        auto timeSinceLastPoor = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastPoorCondition).count();
        if (timeSinceLastPoor >= stabilityWindowMs) {
          // Check if we've already adjusted recently
          auto timeSinceLastAdjustment = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastAdjustment).count();
          if (timeSinceLastAdjustment < ADJUSTMENT_INTERVAL_MS) {
            return std::nullopt;
          }

          int newBitrate = static_cast<int>(currentBitrateKbps * increaseFactor);
          newBitrate = std::min(newBitrate, maxBitrateKbps);

          if (newBitrate != currentBitrateKbps) {
            BOOST_LOG(info) << "AutoBitrate: Good network detected (" << metrics.frameLossPercent
                            << "% loss), increasing bitrate from " << currentBitrateKbps << " to " << newBitrate << " kbps";
            currentBitrateKbps = newBitrate;
            metrics.lastAdjustment = now;
            metrics.consecutiveGoodIntervals = 0;
            metrics.consecutivePoorIntervals = 0;  // Reset poor intervals counter for consistency
            return newBitrate;
          }
        }
      }
    }
    // Stable zone (1-5% loss): maintain current bitrate
    // No adjustment needed

    return std::nullopt;
  }

  void AutoBitrateController::reset(int newBaseBitrate) {
    baseBitrateKbps = newBaseBitrate;
    currentBitrateKbps = newBaseBitrate;
    metrics.consecutiveGoodIntervals = 0;
    metrics.consecutivePoorIntervals = 0;
    metrics.frameLossPercent = 0.0f;
    metrics.lastAdjustment = std::chrono::steady_clock::now();
    metrics.lastPoorCondition = std::chrono::steady_clock::now();
    lastCheckTime = std::chrono::steady_clock::now();
  }

}  // namespace auto_bitrate


