/**
 * @file src/platform/macos/touchscreen.h
 * @brief Real macOS touch for a streamed display: a virtual HID touchscreen (in apollo-touchd) tied to it.
 */
#pragma once

#include <cstdint>
#include <string>

namespace platf::touchscreen {
  /**
   * @brief Whether a virtual touchscreen can be made here (apollo-touchd found and allowed to make one).
   * Checked once and remembered; a failed check is retried after a while.
   */
  bool available();

  /**
   * @brief Applies one touch event from the client to the touchscreen on the given display.
   * @param display_id The CGDirectDisplayID being streamed.
   * @param event_type LI_TOUCH_EVENT_* value.
   * @param pointer_id The client's pointer id.
   * @param x, y Position normalised to the display, 0..1.
   * @param major, minor Contact size normalised to the display, 0..1.
   */
  void update(uint32_t display_id, uint8_t event_type, uint32_t pointer_id, float x, float y, float major, float minor);

  /**
   * @brief Lifts every contact and removes the touchscreen (end of stream).
   */
  void reset();
}  // namespace platf::touchscreen
