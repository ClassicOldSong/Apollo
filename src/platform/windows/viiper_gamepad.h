/**
 * @file src/platform/windows/viiper_gamepad.h
 * @brief Nintendo Switch 2 Pro Controller emulation through libVIIPER and USBIP.
 */
#pragma once

// standard includes
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

// local includes
#include "src/platform/common.h"

namespace platf::viiper {
  /**
   * @brief Report whether a gamepad allocation should use the VIIPER Switch 2 Pro backend.
   *
   * @param configured_gamepad Configured gamepad type.
   * @param client_type Controller type reported by the client (`LI_CTYPE_*`).
   * @return True when the type is `switch`, or `auto` with a Nintendo client controller.
   *         Callers fall back to ViGEmBus when libVIIPER or usbip-win2 is unavailable.
   */
  bool should_emulate_ns2pro(std::string_view configured_gamepad, std::uint8_t client_type);

  /**
   * @brief Convert Moonlight button flags and triggers to Switch 2 Pro button bits.
   *
   * With @p by_label false, face buttons are mapped by position, so Moonlight A (south)
   * becomes Nintendo B. With @p by_label true, Moonlight A becomes Nintendo A.
   *
   * @param button_flags Moonlight gamepad button mask.
   * @param left_trigger Left analog trigger value.
   * @param right_trigger Right analog trigger value.
   * @param by_label Keep face-button labels (client controller is a Nintendo pad).
   * @return Switch 2 Pro button bitfield as defined by libVIIPER.
   */
  std::uint32_t ns2pro_buttons(std::uint32_t button_flags, std::uint8_t left_trigger, std::uint8_t right_trigger, bool by_label);

  /**
   * @brief Convert a signed Moonlight stick axis to the 12-bit Switch 2 Pro range.
   *
   * @param value Moonlight stick value, positive right/up.
   * @return Raw value in `0..4095`, centered at `2048`.
   */
  std::uint16_t ns2pro_stick(std::int16_t value);

  /**
   * @brief Convert Moonlight acceleration to raw Switch 2 Pro accelerometer values.
   *
   * @param x SDL-convention X acceleration in m/s^2.
   * @param y SDL-convention Y acceleration in m/s^2.
   * @param z SDL-convention Z acceleration in m/s^2.
   * @return Raw `AccelX`, `AccelY`, and `AccelZ` report values.
   */
  std::array<std::int16_t, 3> ns2pro_accel(float x, float y, float z);

  /**
   * @brief Convert Moonlight angular velocity to raw Switch 2 Pro gyroscope values.
   *
   * @param x SDL-convention X angular velocity in deg/s.
   * @param y SDL-convention Y angular velocity in deg/s.
   * @param z SDL-convention Z angular velocity in deg/s.
   * @return Raw `GyroX`, `GyroY`, and `GyroZ` report values.
   */
  std::array<std::int16_t, 3> ns2pro_gyro(float x, float y, float z);

  /**
   * @brief Decode Switch 2 HD rumble output into Moonlight motor strengths.
   *
   * @param left Left actuator bytes copied from HID output report `0x02`.
   * @param right Right actuator bytes copied from HID output report `0x02`.
   * @return Low-frequency (left) and high-frequency (right) strengths in `0..65535`.
   */
  std::pair<std::uint16_t, std::uint16_t> ns2pro_rumble(const std::array<std::uint8_t, 16> &left, const std::array<std::uint8_t, 16> &right);

  /**
   * @brief Owns the libVIIPER USBIP server and its virtual Switch 2 Pro controllers.
   */
  class context_t {
  public:
    context_t();
    ~context_t();
    context_t(const context_t &) = delete;
    context_t &operator=(const context_t &) = delete;

    /**
     * @brief Create a virtual Switch 2 Pro Controller for a client gamepad.
     *
     * @param id Global and client-relative identifiers.
     * @param feedback_queue Queue for rumble and motion requests sent back to the client.
     * @param client_type Controller type reported by the client (`LI_CTYPE_*`), used for button layout.
     * @return 0 on success, -1 when libVIIPER or USBIP is unavailable.
     */
    int alloc_gamepad(const gamepad_id_t &id, feedback_queue_t feedback_queue, std::uint8_t client_type);

    /**
     * @brief Route an existing controller's feedback to a resumed client.
     *
     * @param id Global and client-relative identifiers.
     * @param feedback_queue Queue for the resumed client.
     * @return 0 when the controller exists, otherwise -1.
     */
    int rebind_gamepad(const gamepad_id_t &id, feedback_queue_t feedback_queue);

    /**
     * @brief Check whether a global gamepad index is owned by this backend.
     *
     * @param nr Global gamepad index.
     * @return True when a VIIPER controller exists for the index.
     */
    bool has_gamepad(int nr) const;

    /**
     * @brief Remove a virtual controller.
     *
     * @param nr Global gamepad index.
     */
    void free_gamepad(int nr);

    /**
     * @brief Submit button, trigger, and stick state.
     *
     * @param nr Global gamepad index.
     * @param state Moonlight gamepad state.
     */
    void update(int nr, const gamepad_state_t &state);

    /**
     * @brief Submit accelerometer or gyroscope data.
     *
     * @param motion Moonlight motion event.
     */
    void motion(const gamepad_motion_t &motion);

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;  ///< libVIIPER state.
  };
}  // namespace platf::viiper
