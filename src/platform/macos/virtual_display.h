/**
 * @file src/platform/macos/virtual_display.h
 * @brief Virtual displays on macOS (CGVirtualDisplay), the counterpart of the SudoVDA driver on Windows.
 */
#pragma once

#include <cstdint>
#include <string>

namespace VDISPLAY {
  enum class DRIVER_STATUS {
    UNKNOWN = 1,
    OK = 0,
    FAILED = -1,
    VERSION_INCOMPATIBLE = -2,
    WATCHDOG_FAILED = -3
  };

  /**
   * @brief Checks that CGVirtualDisplay is available on this system.
   */
  DRIVER_STATUS openVDisplayDevice();
  void closeVDisplayDevice();

  /**
   * @brief Creates a HiDPI virtual display whose backing store is exactly width x height pixels.
   * @param client_uid Stable id of the client, used for the display's serial number so macOS remembers its arrangement.
   * @param client_name Shown as the display's name.
   * @param fps Refresh rate in millihertz (60000 = 60 Hz).
   * @param scale Points per pixel denominator: 2 gives a HiDPI "looks like" of half the pixel size, 1 a plain mode.
   * @return The display's CGDirectDisplayID as a decimal string, or an empty string on failure.
   */
  std::string createVirtualDisplay(const char *client_uid, const char *client_name, uint32_t width, uint32_t height, uint32_t fps, uint32_t scale = 2);

  /**
   * @brief Removes the virtual display made for this client (if any) and restores any displays it turned off.
   */
  bool removeVirtualDisplay(const char *client_uid);

  /**
   * @brief Removes every virtual display this process made.
   */
  void removeAllVirtualDisplays();

  /**
   * @brief Turns off every other display so the given one is the only (and main) display.
   * The change is made for this process only: macOS puts the displays back if the process exits.
   */
  bool isolateDisplay(uint32_t display_id);

  /**
   * @brief Turns back on the displays isolateDisplay() turned off.
   */
  void restoreDisplays();

  /**
   * @brief The display's CoreGraphics UUID string, which a HID touchscreen names in its displayUUID property.
   */
  std::string displayUUID(uint32_t display_id);

  /**
   * @brief isolateDisplay() starts a copy of the host as `<exe> --display-guard <pid> <display ids>`,
   * which turns the displays back on if the host dies. main() hands that invocation to displayGuardMain().
   */
  bool isDisplayGuardInvocation(int argc, char **argv);
  int displayGuardMain(int argc, char **argv);
}  // namespace VDISPLAY
