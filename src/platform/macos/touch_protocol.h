/**
 * @file src/platform/macos/touch_protocol.h
 * @brief Messages between Apollo and apollo-touchd, the helper that owns the virtual HID touchscreen.
 *
 * Making a virtual HID device needs the com.apple.developer.hid.virtual.device entitlement, which only a
 * binary signed with a matching provisioning profile may claim. So the device lives in a small signed
 * helper app; Apollo starts it with one end of a datagram socketpair on fd 3 and sends these messages.
 * The helper exits, and the device disappears, when Apollo closes the socket or dies.
 */
#pragma once

#include <cstdint>

namespace touch_protocol {
  constexpr int helper_fd = 3;
  constexpr int max_contacts = 10;
  constexpr uint16_t axis_max = 32767;

  enum msg_type_e : uint8_t {
    create = 1,  ///< create_msg_t; answered with reply_msg_t
    frame = 2,  ///< frame_msg_t: the full set of contacts now on the screen
    destroy = 3,  ///< no body; answered with reply_msg_t
  };

#pragma pack(push, 1)
  struct create_msg_t {
    uint8_t type;  ///< msg_type_e::create
    char display_uuid[40];  ///< CoreGraphics UUID string of the display the touchscreen belongs to
    uint16_t width_mm;  ///< Physical size reported for the surface
    uint16_t height_mm;
  };

  struct contact_t {
    uint8_t id;  ///< Contact identifier, stable for the life of a touch
    uint8_t flags;  ///< bit 0: tip switch (touching), bit 1: in range
    uint16_t x;  ///< 0..axis_max across the display
    uint16_t y;
    uint16_t width;  ///< Contact size, same units as x
    uint16_t height;
  };

  struct frame_msg_t {
    uint8_t type;  ///< msg_type_e::frame
    uint8_t count;
    contact_t contacts[max_contacts];
  };

  struct reply_msg_t {
    uint8_t ok;
    char error[127];
  };
#pragma pack(pop)
}  // namespace touch_protocol
