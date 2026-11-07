/**
 * @file src/input.h
 * @brief Declarations for gamepad, keyboard, and mouse input handling.
 */
#pragma once

// standard includes
#include <functional>
#include <utility>

// local includes
#include "platform/common.h"
#include "thread_safe.h"
#include "crypto.h"

namespace input {
  struct input_t;

  void print(void *input);
  void reset(std::shared_ptr<input_t> &input);
  void passthrough(std::shared_ptr<input_t> &input, std::vector<std::uint8_t> &&input_data, const crypto::PERM& permission);

  [[nodiscard]] std::unique_ptr<platf::deinit_t> init();

  bool probe_gamepads();

  std::shared_ptr<input_t> alloc(safe::mail_t mail);

  /**
   * @brief Placement of the host display's image inside the client stream.
   *
   * The encoder aspect-fits the host display into the client stream (config.width x config.height),
   * adding letterbox/pillarbox bars when the aspect ratios differ. All values are in client stream pixels,
   * except the scale ratios which convert stream pixels into host display pixels.
   */
  struct touch_scale_t {
    float scale_x, scale_y;  ///< Host display pixels per client stream pixel (display size / video content size).
    float content_x, content_y;  ///< Top-left corner of the video content inside the stream.
    float content_width, content_height;  ///< Size of the video content inside the stream.
  };

  /**
   * @brief Compute the aspect-fit placement and scale ratios of a host display inside a client stream.
   * @param stream_width Width of the client stream (config.width).
   * @param stream_height Height of the client stream (config.height).
   * @param display_width Width of the captured host display.
   * @param display_height Height of the captured host display.
   * @return The scale ratios and active video area. Returns an identity mapping for invalid sizes.
   */
  touch_scale_t compute_touch_scale(int stream_width, int stream_height, int display_width, int display_height);

  struct touch_port_t: public platf::touch_port_t {
    // offset_x/offset_y: position of the captured display in the host environment (virtual desktop)
    // width/height: size of the client stream

    // Size of the whole host environment (virtual desktop)
    int env_width, env_height;

    // Size of the captured host display
    int display_width, display_height;

    // Mapping from client stream pixels to host display pixels
    touch_scale_t scale;

    explicit operator bool() const {
      return width > 0 && height > 0 && env_width > 0 && env_height > 0 && display_width > 0 && display_height > 0;
    }
  };

  /**
   * @brief Build the touch port for a captured display and a client stream size.
   */
  touch_port_t make_touch_port(int offset_x, int offset_y, int stream_width, int stream_height, int display_width, int display_height, int env_width, int env_height);

  /**
   * @brief Map normalized client coordinates to display-local host pixels.
   *
   * The client reports positions normalized (0..1) against its whole stream surface, letterbox bars included.
   * Positions are clamped to the active video area, then scaled by the per-axis ratios of `port.scale`,
   * so (0,0) maps to (0,0) and (1,1) maps to (display_width, display_height) when there are no bars.
   *
   * @param port The touch port.
   * @param x Normalized client X coordinate (0..1).
   * @param y Normalized client Y coordinate (0..1).
   * @return Display-local host pixel coordinates in [0, display_width] x [0, display_height].
   */
  std::pair<float, float> client_to_display(const touch_port_t &port, float x, float y);

  /**
   * @brief Scale of a normalized client contact area into host display pixels, per axis.
   *
   * Contact areas are normalized against the client stream surface, so one unit on each axis
   * equals the stream size times the stream-to-display scale ratio of that axis.
   */
  std::pair<float, float> client_contact_area_scalar(const touch_port_t &port);

  /**
   * @brief The port handed to platf::abs_mouse().
   *
   * It is {offset_x, offset_y, env_width, env_height}: the display offset inside the host
   * environment and the size of the whole environment. abs_mouse() receives display-local host pixels.
   */
  platf::touch_port_t platform_touch_port(const touch_port_t &port);

  /**
   * @brief The port handed to platf::touch_update()/pen_update().
   *
   * Platform contract (relied on by the Windows and Linux backends):
   *  - the port is {offset_x, offset_y, display_width, display_height}: the streamed display's
   *    offset inside the host environment and its size in host pixels;
   *  - touch.x/pen.x = display-local host pixel / display_width, touch.y/pen.y = display-local host pixel / display_height,
   *    i.e. normalized against the streamed display, so host pixel = x * port.width + port.offset_x;
   *  - contactAreaMajor/Minor are in host pixels (see client_contact_area_scalar()).
   *
   * Compositors bind an absolute touch/pen device to a single output, so normalizing against
   * the streamed display (not the whole environment) keeps touches on that display.
   */
  platf::touch_port_t platform_display_port(const touch_port_t &port);

  /**
   * @brief Scale the ellipse axes according to the provided size.
   * @param val The major and minor axis pair.
   * @param rotation The rotation value from the touch/pen event.
   * @param scalar The scalar cartesian coordinate pair.
   * @return The major and minor axis pair.
   */
  std::pair<float, float> scale_client_contact_area(const std::pair<float, float> &val, uint16_t rotation, const std::pair<float, float> &scalar);
}  // namespace input
