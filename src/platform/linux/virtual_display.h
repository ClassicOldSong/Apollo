/**
 * @file src/platform/linux/virtual_display.h
 * @brief Virtual display declarations for Linux.
 */
#pragma once

// standard includes
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// local includes
#include "src/uuid.h"

namespace VDISPLAY {

  /**
   * @brief Linux virtual display backend implementation.
   */
  enum class BACKEND {
    UNKNOWN,
    EVDI_PIPEWIRE,
    MUTTER_PIPEWIRE,
    EVDI
  };

  /**
   * @brief Status of the virtual display driver.
   */
  enum class DRIVER_STATUS {
    UNKNOWN = 1,  ///< Driver status unknown
    OK = 0,  ///< Driver is operational
    FAILED = -1,  ///< Driver failed to initialize
    VERSION_INCOMPATIBLE = -2,  ///< Driver version incompatible
    WATCHDOG_FAILED = -3,  ///< Driver watchdog failed
    NOT_SUPPORTED = -4  ///< Virtual display not supported on this system
  };

  /**
   * @brief Initialize the virtual display driver.
   * @return DRIVER_STATUS indicating the result of initialization.
   */
  DRIVER_STATUS openVDisplayDevice();

  /**
   * @brief Close the virtual display driver.
   */
  void closeVDisplayDevice();

  /**
   * @brief Start a ping thread to keep the virtual display alive.
   * @param failCb Callback to invoke if the watchdog fails.
   * @return true if the ping thread was started successfully, false otherwise.
   */
  bool startPingThread(std::function<void()> failCb);

  /**
   * @brief Set the render adapter by name.
   * @param adapterName The name of the adapter to use for rendering.
   * @return true if the adapter was set successfully, false otherwise.
   */
  bool setRenderAdapterByName(const std::string &adapterName);

  /**
   * @brief Create a virtual display.
   * @param s_client_uid The unique identifier of the client.
   * @param s_client_name The name of the client.
   * @param width The width of the virtual display.
   * @param height The height of the virtual display.
   * @param fps The refresh rate of the virtual display (in mHz).
   * @param guid The GUID for the virtual display.
   * @return The name of the created virtual display, or empty string on failure.
   */
  std::string createVirtualDisplay(
    const char *s_client_uid,
    const char *s_client_name,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    const uuid_util::uuid_t &guid
  );

  /**
   * @brief Remove a virtual display.
   * @param guid The GUID of the virtual display to remove.
   * @return true if the virtual display was removed successfully, false otherwise.
   */
  bool removeVirtualDisplay(const uuid_util::uuid_t &guid);

  /**
   * @brief Change the display settings of a virtual display.
   * @param deviceName The name of the virtual display.
   * @param width The new width.
   * @param height The new height.
   * @param refresh_rate The new refresh rate (in mHz).
   * @return 0 on success, non-zero on failure.
   */
  int changeDisplaySettings(const char *deviceName, int width, int height, int refresh_rate);

  /**
   * @brief Change the display settings with isolated display option.
   * @param deviceName The name of the virtual display.
   * @param width The new width.
   * @param height The new height.
   * @param refresh_rate The new refresh rate (in mHz).
   * @param bApplyIsolated Whether to apply isolated display settings.
   * @return 0 on success, non-zero on failure.
   */
  int changeDisplaySettings2(const char *deviceName, int width, int height, int refresh_rate, bool bApplyIsolated = false);

  /**
   * @brief Get the primary display name.
   * @return The name of the primary display.
   */
  std::string getPrimaryDisplay();

  /**
   * @brief Set the primary display by name.
   * @param primaryDeviceName The name of the display to set as primary.
   * @return true if the primary display was set successfully, false otherwise.
   */
  bool setPrimaryDisplay(const char *primaryDeviceName);

  /**
   * @brief Get the HDR status of a display by name.
   * @param displayName The name of the display.
   * @return true if HDR is enabled, false otherwise.
   */
  bool getDisplayHDRByName(const char *displayName);

  /**
   * @brief Set the HDR status of a display by name.
   * @param displayName The name of the display.
   * @param enableAdvancedColor Whether to enable HDR.
   * @return true if the HDR status was set successfully, false otherwise.
   */
  bool setDisplayHDRByName(const char *displayName, bool enableAdvancedColor);

  /**
   * @brief Match displays by a given pattern.
   * @param sMatch The pattern to match.
   * @return A vector of matching display names.
   */
  std::vector<std::string> matchDisplay(const std::string &sMatch);

  /**
   * @brief Check whether a display name belongs to an Apollo virtual display.
   * @param displayName The display name to check.
   * @return true if the display is an active Apollo virtual display.
   */
  bool isVirtualDisplay(const std::string &displayName);

  /**
   * @brief Get the backend that owns an Apollo virtual display.
   * @param displayName The display name to check.
   * @return The backend, or UNKNOWN if the display is not registered.
   */
  BACKEND virtualDisplayBackend(const std::string &displayName);

  /**
   * @brief Get the current requested mode for an Apollo virtual display.
   * @param displayName The virtual display name.
   * @param width Output width.
   * @param height Output height.
   * @param fps Output refresh rate in mHz.
   * @return true if the mode was found.
   */
  bool getVirtualDisplayMode(const std::string &displayName, uint32_t &width, uint32_t &height, uint32_t &fps);

  /**
   * @brief Get the PipeWire node id for a Mutter-owned virtual display.
   * @param displayName The virtual display name.
   * @param node_id Output PipeWire node id.
   * @return true if the node id is available.
   */
  bool getMutterPipeWireNodeId(const std::string &displayName, uint32_t &node_id);

  /**
   * @brief Send relative pointer motion to the active Mutter remote desktop virtual display.
   * @param dx Horizontal movement delta.
   * @param dy Vertical movement delta.
   * @return true if the event was queued to Mutter.
   */
  bool notifyMutterPointerMotionRelative(double dx, double dy);

  /**
   * @brief Send absolute pointer motion to the active Mutter remote desktop virtual display.
   * @param x Stream-relative X coordinate.
   * @param y Stream-relative Y coordinate.
   * @return true if the event was queued to Mutter.
   */
  bool notifyMutterPointerMotionAbsolute(double x, double y);

  /**
   * @brief Send a pointer button event to the active Mutter remote desktop virtual display.
   * @param button Linux input button code.
   * @param release true when releasing the button, false when pressing it.
   * @return true if the event was queued to Mutter.
   */
  bool notifyMutterPointerButton(int button, bool release);

  /**
   * @brief Send a pointer axis event to the active Mutter remote desktop virtual display.
   * @param dx Horizontal axis delta.
   * @param dy Vertical axis delta.
   * @return true if the event was queued to Mutter.
   */
  bool notifyMutterPointerAxis(double dx, double dy);

  /**
   * @brief Check if a display is an EVDI virtual display.
   * @param displayName The name of the display to check.
   * @return true if the display is an EVDI virtual display, false otherwise.
   */
  bool isEvdiDisplay(const std::string &displayName);

  /**
   * @brief Get the DRM card index for an EVDI display.
   * @param displayName The name of the EVDI display.
   * @return The card index, or -1 if not found or not an EVDI display.
   */
  int getEvdiCardIndex(const std::string &displayName);

  /**
   * @brief Copy the latest EVDI painter frame into a caller-owned buffer.
   * @param displayName The EVDI display name.
   * @param dst Destination buffer.
   * @param width Expected frame width.
   * @param height Expected frame height.
   * @param dst_stride Destination row pitch in bytes.
   * @param timeout Maximum time to wait for a frame newer than last_generation.
   * @param last_generation Last generation consumed by the caller; updated on success.
   * @param frame_timestamp Timestamp for the copied frame; updated on success.
   * @return true if a new frame was copied, false otherwise.
   */
  bool copyLatestEvdiFrame(
    const std::string &displayName,
    std::uint8_t *dst,
    int width,
    int height,
    int dst_stride,
    std::chrono::milliseconds timeout,
    std::uint64_t &last_generation,
    std::chrono::steady_clock::time_point &frame_timestamp,
    bool require_new_frame = true
  );

  /**
   * @brief Check whether the EVDI painter is blocked in a grab operation.
   * @param displayName The EVDI display name.
   * @param minimum_duration Minimum grab age to report as busy.
   * @return true if the current EVDI grab has exceeded minimum_duration.
   */
  bool isEvdiGrabBusy(const std::string &displayName, std::chrono::milliseconds minimum_duration);

}  // namespace VDISPLAY
