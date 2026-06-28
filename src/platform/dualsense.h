/**
 * @file src/platform/dualsense.h
 * @brief Shared helpers for packing DualSense controller reports.
 */
#pragma once

// standard includes
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

// local includes
#include "src/platform/common.h"
#include "src/platform/usb.h"

namespace platf::dualsense {
  constexpr std::uint16_t VENDOR_ID_SONY = 0x054C;
  constexpr std::uint16_t PRODUCT_ID_DUALSENSE = 0x0CE6;
  constexpr std::uint16_t PRODUCT_ID_DUALSENSE_EDGE = 0x0DF2;

  constexpr std::uint8_t INPUT_REPORT_USB = 0x01;
  constexpr std::uint8_t OUTPUT_REPORT_USB = 0x02;
  constexpr std::size_t INPUT_REPORT_USB_SIZE = 64;
  constexpr std::size_t INPUT_REPORT_BODY_SIZE = INPUT_REPORT_USB_SIZE - 1;
  constexpr std::size_t OUTPUT_REPORT_USB_SIZE = 64;
  constexpr std::size_t OUTPUT_REPORT_BODY_SIZE = OUTPUT_REPORT_USB_SIZE - 1;
  constexpr std::size_t OUTPUT_REPORT_COMMON_SIZE = 47;
  constexpr std::size_t OUTPUT_REPORT_USB_MIN_SIZE = 1 + OUTPUT_REPORT_COMMON_SIZE;
  constexpr std::size_t DUALSENSE_HID_REPORT_DESCRIPTOR_SIZE = 273;
  constexpr std::size_t DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE = 389;
  constexpr std::size_t DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR_SIZE = 227;
  constexpr std::size_t DUALSENSE_EDGE_HID_DESCRIPTOR_OFFSET = 204;
  constexpr std::size_t DUALSENSE_EDGE_HID_INTERRUPT_IN_ENDPOINT_DESCRIPTOR_OFFSET = 213;
  constexpr std::size_t DUALSENSE_EDGE_HID_INTERRUPT_OUT_ENDPOINT_DESCRIPTOR_OFFSET = 220;
  constexpr std::uint8_t DUALSENSE_EDGE_HID_INTERRUPT_IN_INTERVAL = 0x04;
  constexpr std::uint8_t DUALSENSE_EDGE_HID_INTERRUPT_OUT_INTERVAL = 0x06;
  constexpr std::uint8_t DUALSENSE_EDGE_HID_INTERRUPT_IN_INTERVAL_MS = 1;
  constexpr std::uint8_t DUALSENSE_EDGE_INTERFACE_AUDIO_CONTROL = 0;
  constexpr std::uint8_t DUALSENSE_EDGE_INTERFACE_AUDIO_OUT = 1;
  constexpr std::uint8_t DUALSENSE_EDGE_INTERFACE_AUDIO_IN = 2;
  constexpr std::uint8_t DUALSENSE_EDGE_INTERFACE_HID = 3;
  constexpr std::uint8_t USB_ENDPOINT_AUDIO_OUT = 0x01;
  constexpr std::uint8_t USB_ENDPOINT_AUDIO_IN = 0x82;
  constexpr std::uint8_t USB_ENDPOINT_IN = 0x84;
  constexpr std::uint8_t USB_ENDPOINT_OUT = 0x03;
  constexpr std::string_view DUALSENSE_EDGE_MANUFACTURER = "Sony Interactive Entertainment";
  constexpr std::string_view DUALSENSE_EDGE_PRODUCT = "DualSense Edge Wireless Controller";
  constexpr std::uint8_t FEATURE_REPORT_CALIBRATION = 0x05;
  constexpr std::uint8_t FEATURE_REPORT_PAIRING_INFO = 0x09;
  constexpr std::uint8_t FEATURE_REPORT_FIRMWARE_INFO = 0x20;
  constexpr std::size_t FEATURE_REPORT_CALIBRATION_SIZE = 41;
  constexpr std::size_t FEATURE_REPORT_PAIRING_INFO_SIZE = 20;
  constexpr std::size_t FEATURE_REPORT_FIRMWARE_INFO_SIZE = 64;

  inline constexpr std::array<std::uint8_t, 18> DUALSENSE_EDGE_DEVICE_DESCRIPTOR {
    0x12,  // bLength
    0x01,  // bDescriptorType: device
    0x00, 0x02,  // bcdUSB 2.00
    0x00,  // bDeviceClass
    0x00,  // bDeviceSubClass
    0x00,  // bDeviceProtocol
    0x40,  // bMaxPacketSize0
    0x4C, 0x05,  // idVendor: Sony
    0xF2, 0x0D,  // idProduct: DualSense Edge
    0x00, 0x01,  // bcdDevice 1.00
    0x01,  // iManufacturer
    0x02,  // iProduct
    0x00,  // iSerialNumber
    0x01,  // bNumConfigurations
  };

  // Captured from the wired DualSense Edge configuration descriptor in
  // hhd-dev/hwinfo's ps5_edge_sample.pcapng USB dump. Keeping the real
  // composite layout gives Windows the same MI_03 HID instance path used by
  // DS5-aware tooling, while Apollo still implements only the HID interface.
  inline constexpr std::array<std::uint8_t, DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR_SIZE> DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR {
    0x09,  // bLength
    0x02,  // bDescriptorType: configuration
    0xE3, 0x00,  // wTotalLength
    0x04,  // bNumInterfaces
    0x01,  // bConfigurationValue
    0x00,  // iConfiguration
    0xC0,  // bmAttributes: self-powered
    0xFA,  // bMaxPower: 500 mA
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x0A, 0x24, 0x01, 0x00, 0x01, 0x49, 0x00, 0x02, 0x01, 0x02,
    0x0C, 0x24, 0x02, 0x01, 0x01, 0x01, 0x06, 0x04, 0x33, 0x00, 0x00, 0x00,
    0x0C, 0x24, 0x06, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x24, 0x03, 0x03, 0x01, 0x03, 0x04, 0x02, 0x00,
    0x0C, 0x24, 0x02, 0x04, 0x02, 0x04, 0x03, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x24, 0x06, 0x05, 0x04, 0x01, 0x03, 0x00, 0x00,
    0x09, 0x24, 0x03, 0x06, 0x01, 0x01, 0x01, 0x05, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x0B, 0x24, 0x02, 0x01, 0x04, 0x02, 0x10, 0x01, 0x80, 0xBB, 0x00,
    0x09, 0x05, USB_ENDPOINT_AUDIO_OUT, 0x09, 0x88, 0x01, 0x04, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x06, 0x01, 0x01, 0x00,
    0x0B, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01, 0x80, 0xBB, 0x00,
    0x09, 0x05, USB_ENDPOINT_AUDIO_IN, 0x05, 0xC4, 0x00, 0x04, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x04, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x85, 0x01,
    0x07, 0x05, USB_ENDPOINT_IN, 0x03, 0x40, 0x00, 0x04,
    0x07, 0x05, USB_ENDPOINT_OUT, 0x03, 0x40, 0x00, 0x06,
  };

  static_assert(DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR[2] == (DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR_SIZE & 0xFF));
  static_assert(DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR[3] == (DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR_SIZE >> 8));
  static_assert(DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR[4] == 4);
  static_assert(DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR[DUALSENSE_EDGE_HID_DESCRIPTOR_OFFSET] == 0x09);
  static_assert(DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR[DUALSENSE_EDGE_HID_DESCRIPTOR_OFFSET + 1] == 0x21);

  // Captured from a wired DualSense Edge (054c:0df2) in hhd-dev/hwinfo's
  // ps5edge_LFnRFnLBRB.txt hardware dump.
  inline constexpr std::array<std::uint8_t, DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE> DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,
    0x09, 0x33, 0x09, 0x34, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02, 0x06,
    0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00, 0x05,
    0x09, 0x19, 0x01, 0x29, 0x0F, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02, 0x06,
    0x00, 0xFF, 0x09, 0x21, 0x95, 0x0D, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09, 0x22, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x75, 0x08, 0x95, 0x34, 0x81, 0x02, 0x85, 0x02, 0x09, 0x23, 0x95, 0x3F, 0x91, 0x02,
    0x85, 0x05, 0x09, 0x33, 0x95, 0x28, 0xB1, 0x02, 0x85, 0x08, 0x09, 0x34, 0x95, 0x2F, 0xB1, 0x02,
    0x85, 0x09, 0x09, 0x24, 0x95, 0x13, 0xB1, 0x02, 0x85, 0x0A, 0x09, 0x25, 0x95, 0x1A, 0xB1, 0x02,
    0x85, 0x20, 0x09, 0x26, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x21, 0x09, 0x27, 0x95, 0x04, 0xB1, 0x02,
    0x85, 0x22, 0x09, 0x40, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x80, 0x09, 0x28, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x81, 0x09, 0x29, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x82, 0x09, 0x2A, 0x95, 0x09, 0xB1, 0x02,
    0x85, 0x83, 0x09, 0x2B, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x84, 0x09, 0x2C, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x85, 0x09, 0x2D, 0x95, 0x02, 0xB1, 0x02, 0x85, 0xA0, 0x09, 0x2E, 0x95, 0x01, 0xB1, 0x02,
    0x85, 0xE0, 0x09, 0x2F, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF0, 0x09, 0x30, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0xF1, 0x09, 0x31, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF2, 0x09, 0x32, 0x95, 0x34, 0xB1, 0x02,
    0x85, 0xF4, 0x09, 0x35, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF5, 0x09, 0x36, 0x95, 0x03, 0xB1, 0x02,
    0x85, 0x60, 0x09, 0x41, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x61, 0x09, 0x42, 0xB1, 0x02, 0x85, 0x62,
    0x09, 0x43, 0xB1, 0x02, 0x85, 0x63, 0x09, 0x44, 0xB1, 0x02, 0x85, 0x64, 0x09, 0x45, 0xB1, 0x02,
    0x85, 0x65, 0x09, 0x46, 0xB1, 0x02, 0x85, 0x68, 0x09, 0x47, 0xB1, 0x02, 0x85, 0x70, 0x09, 0x48,
    0xB1, 0x02, 0x85, 0x71, 0x09, 0x49, 0xB1, 0x02, 0x85, 0x72, 0x09, 0x4A, 0xB1, 0x02, 0x85, 0x73,
    0x09, 0x4B, 0xB1, 0x02, 0x85, 0x74, 0x09, 0x4C, 0xB1, 0x02, 0x85, 0x75, 0x09, 0x4D, 0xB1, 0x02,
    0x85, 0x76, 0x09, 0x4E, 0xB1, 0x02, 0x85, 0x77, 0x09, 0x4F, 0xB1, 0x02, 0x85, 0x78, 0x09, 0x50,
    0xB1, 0x02, 0x85, 0x79, 0x09, 0x51, 0xB1, 0x02, 0x85, 0x7A, 0x09, 0x52, 0xB1, 0x02, 0x85, 0x7B,
    0x09, 0x53, 0xB1, 0x02, 0xC0
  };

  constexpr std::uint8_t HAT_NORTH = 0x00;
  constexpr std::uint8_t HAT_NORTHEAST = 0x01;
  constexpr std::uint8_t HAT_EAST = 0x02;
  constexpr std::uint8_t HAT_SOUTHEAST = 0x03;
  constexpr std::uint8_t HAT_SOUTH = 0x04;
  constexpr std::uint8_t HAT_SOUTHWEST = 0x05;
  constexpr std::uint8_t HAT_WEST = 0x06;
  constexpr std::uint8_t HAT_NORTHWEST = 0x07;
  constexpr std::uint8_t HAT_NONE = 0x08;

  constexpr std::uint8_t BUTTONS0_SQUARE = 0x10;
  constexpr std::uint8_t BUTTONS0_CROSS = 0x20;
  constexpr std::uint8_t BUTTONS0_CIRCLE = 0x40;
  constexpr std::uint8_t BUTTONS0_TRIANGLE = 0x80;

  constexpr std::uint8_t BUTTONS1_L1 = 0x01;
  constexpr std::uint8_t BUTTONS1_R1 = 0x02;
  constexpr std::uint8_t BUTTONS1_L2 = 0x04;
  constexpr std::uint8_t BUTTONS1_R2 = 0x08;
  constexpr std::uint8_t BUTTONS1_CREATE = 0x10;
  constexpr std::uint8_t BUTTONS1_OPTIONS = 0x20;
  constexpr std::uint8_t BUTTONS1_L3 = 0x40;
  constexpr std::uint8_t BUTTONS1_R3 = 0x80;

  constexpr std::uint8_t BUTTONS2_PS_HOME = 0x01;
  constexpr std::uint8_t BUTTONS2_TOUCHPAD = 0x02;
  constexpr std::uint8_t BUTTONS2_MIC_MUTE = 0x04;
  constexpr std::uint8_t BUTTONS2_EDGE_FN_LEFT = 0x10;
  constexpr std::uint8_t BUTTONS2_EDGE_FN_RIGHT = 0x20;
  constexpr std::uint8_t BUTTONS2_EDGE_LEFT_PADDLE = 0x40;
  constexpr std::uint8_t BUTTONS2_EDGE_RIGHT_PADDLE = 0x80;

  constexpr std::uint8_t OUTPUT_VALID_FLAG0_COMPATIBLE_VIBRATION = 0x01;
  constexpr std::uint8_t OUTPUT_VALID_FLAG0_HAPTICS_SELECT = 0x02;
  constexpr std::uint8_t OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE = 0x04;
  constexpr std::uint8_t OUTPUT_VALID_FLAG2_COMPATIBLE_VIBRATION2 = 0x04;

  struct button_report_t {
    std::uint8_t buttons0;
    std::uint8_t buttons1;
    std::uint8_t buttons2;
  };

  struct input_report_t {
    std::uint8_t x;
    std::uint8_t y;
    std::uint8_t rx;
    std::uint8_t ry;
    std::uint8_t z;
    std::uint8_t rz;
    std::uint8_t sequence_number;
    std::array<std::uint8_t, 4> buttons;
    std::array<std::uint8_t, 4> reserved;
    std::array<std::uint8_t, 6> gyro;
    std::array<std::uint8_t, 6> accel;
    std::array<std::uint8_t, 4> sensor_timestamp;
    std::uint8_t reserved2;
    std::array<std::uint8_t, 8> touch_points;
    std::array<std::uint8_t, 12> reserved3;
    std::array<std::uint8_t, 3> status;
    std::array<std::uint8_t, 8> reserved4;
  };

  struct usb_input_report_t {
    std::uint8_t report_id;
    input_report_t body;
  };

  struct output_report_state_t {
    bool valid;
    bool has_rumble;
    std::uint8_t motor_left;
    std::uint8_t motor_right;
    bool has_lightbar;
    std::uint8_t lightbar_red;
    std::uint8_t lightbar_green;
    std::uint8_t lightbar_blue;
  };

  struct control_response_t {
    bool handled;
    std::vector<std::uint8_t> payload;
  };

  static_assert(sizeof(input_report_t) == INPUT_REPORT_BODY_SIZE);
  static_assert(sizeof(usb_input_report_t) == INPUT_REPORT_USB_SIZE);

  constexpr std::uint8_t pack_dpad(std::uint32_t flags) {
    const bool up = flags & DPAD_UP;
    const bool down = flags & DPAD_DOWN;
    const bool left = flags & DPAD_LEFT;
    const bool right = flags & DPAD_RIGHT;

    if (up) {
      if (right) {
        return HAT_NORTHEAST;
      }
      if (left) {
        return HAT_NORTHWEST;
      }
      return HAT_NORTH;
    }

    if (down) {
      if (right) {
        return HAT_SOUTHEAST;
      }
      if (left) {
        return HAT_SOUTHWEST;
      }
      return HAT_SOUTH;
    }

    if (right) {
      return HAT_EAST;
    }
    if (left) {
      return HAT_WEST;
    }

    return HAT_NONE;
  }

  constexpr std::uint8_t axis_to_byte(std::int16_t value) {
    return static_cast<std::uint8_t>((static_cast<int>(value) + std::numeric_limits<std::uint16_t>::max() / 2 + 1) / 257);
  }

  constexpr std::uint8_t inverted_axis_to_byte(std::int16_t value) {
    const auto new_value = -((std::numeric_limits<std::uint16_t>::max() / 2 + static_cast<int>(value) - 1) / 257);
    return new_value == 0 ? 0xFF : static_cast<std::uint8_t>(new_value);
  }

  constexpr button_report_t pack_buttons(std::uint32_t flags, std::uint8_t left_trigger, std::uint8_t right_trigger, bool dualsense_edge) {
    button_report_t report {
      .buttons0 = pack_dpad(flags),
      .buttons1 = 0,
      .buttons2 = 0,
    };

    if (flags & X) {
      report.buttons0 |= BUTTONS0_SQUARE;
    }
    if (flags & A) {
      report.buttons0 |= BUTTONS0_CROSS;
    }
    if (flags & B) {
      report.buttons0 |= BUTTONS0_CIRCLE;
    }
    if (flags & Y) {
      report.buttons0 |= BUTTONS0_TRIANGLE;
    }

    if (flags & LEFT_BUTTON) {
      report.buttons1 |= BUTTONS1_L1;
    }
    if (flags & RIGHT_BUTTON) {
      report.buttons1 |= BUTTONS1_R1;
    }
    if (left_trigger > 0) {
      report.buttons1 |= BUTTONS1_L2;
    }
    if (right_trigger > 0) {
      report.buttons1 |= BUTTONS1_R2;
    }
    if (flags & BACK) {
      report.buttons1 |= BUTTONS1_CREATE;
    }
    if (flags & START) {
      report.buttons1 |= BUTTONS1_OPTIONS;
    }
    if (flags & LEFT_STICK) {
      report.buttons1 |= BUTTONS1_L3;
    }
    if (flags & RIGHT_STICK) {
      report.buttons1 |= BUTTONS1_R3;
    }

    if (flags & HOME) {
      report.buttons2 |= BUTTONS2_PS_HOME;
    }
    if (flags & TOUCHPAD_BUTTON) {
      report.buttons2 |= BUTTONS2_TOUCHPAD;
    }
    if (flags & MISC_BUTTON) {
      report.buttons2 |= BUTTONS2_MIC_MUTE;
    }

    if (dualsense_edge) {
      // Moonlight normalizes native SDL2 and sdl2-compat/SDL3 Edge raw inputs
      // into PADDLE1 through PADDLE4 before Apollo packs the Edge-only byte.
      if (flags & PADDLE1) {
        report.buttons2 |= BUTTONS2_EDGE_RIGHT_PADDLE;
      }
      if (flags & PADDLE2) {
        report.buttons2 |= BUTTONS2_EDGE_LEFT_PADDLE;
      }
      if (flags & PADDLE3) {
        report.buttons2 |= BUTTONS2_EDGE_FN_RIGHT;
      }
      if (flags & PADDLE4) {
        report.buttons2 |= BUTTONS2_EDGE_FN_LEFT;
      }
    }

    return report;
  }

  constexpr input_report_t make_input_report(const gamepad_state_t &gamepad_state, std::uint8_t sequence_number, bool dualsense_edge) {
    auto buttons = pack_buttons(gamepad_state.buttonFlags, gamepad_state.lt, gamepad_state.rt, dualsense_edge);

    input_report_t report {};
    report.x = axis_to_byte(gamepad_state.lsX);
    report.y = inverted_axis_to_byte(gamepad_state.lsY);
    report.rx = axis_to_byte(gamepad_state.rsX);
    report.ry = inverted_axis_to_byte(gamepad_state.rsY);
    report.z = gamepad_state.lt;
    report.rz = gamepad_state.rt;
    report.sequence_number = sequence_number;
    report.buttons = {buttons.buttons0, buttons.buttons1, buttons.buttons2, 0};
    report.touch_points[0] = 0x80;
    report.touch_points[4] = 0x80;

    return report;
  }

  constexpr usb_input_report_t make_usb_input_report(const gamepad_state_t &gamepad_state, std::uint8_t sequence_number, bool dualsense_edge) {
    return usb_input_report_t {
      .report_id = INPUT_REPORT_USB,
      .body = make_input_report(gamepad_state, sequence_number, dualsense_edge),
    };
  }

  inline std::vector<std::uint8_t> report_bytes(const usb_input_report_t &report) {
    const auto *data = reinterpret_cast<const std::uint8_t *>(&report);
    return {data, data + sizeof(report)};
  }

  inline output_report_state_t parse_output_report(std::span<const std::uint8_t> report) {
    if (report.size() < OUTPUT_REPORT_USB_MIN_SIZE || report.front() != OUTPUT_REPORT_USB) {
      return {};
    }

    const auto common = report.subspan(1, OUTPUT_REPORT_COMMON_SIZE);
    return output_report_state_t {
      .valid = true,
      .has_rumble = static_cast<bool>((common[0] & (OUTPUT_VALID_FLAG0_COMPATIBLE_VIBRATION | OUTPUT_VALID_FLAG0_HAPTICS_SELECT)) ||
                                      (common[38] & OUTPUT_VALID_FLAG2_COMPATIBLE_VIBRATION2)),
      .motor_left = common[3],
      .motor_right = common[2],
      .has_lightbar = static_cast<bool>(common[1] & OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE),
      .lightbar_red = common[44],
      .lightbar_green = common[45],
      .lightbar_blue = common[46],
    };
  }

  constexpr bool dualsense_edge_targets_hid_interface(std::uint16_t interface_index) {
    return interface_index == DUALSENSE_EDGE_INTERFACE_HID;
  }

  constexpr bool dualsense_edge_has_interface(std::uint16_t interface_index) {
    return interface_index <= DUALSENSE_EDGE_INTERFACE_HID;
  }

  constexpr bool dualsense_edge_accepts_alternate_setting(std::uint16_t interface_index, std::uint16_t alternate_setting) {
    return dualsense_edge_has_interface(interface_index) && alternate_setting == 0;
  }

  constexpr bool dualsense_edge_accepts_configuration(std::uint16_t configuration_value) {
    return configuration_value == 0 || configuration_value == 1;
  }

  constexpr bool dualsense_edge_has_endpoint(std::uint16_t endpoint_index) {
    return endpoint_index == 0 ||
           endpoint_index == USB_ENDPOINT_AUDIO_OUT ||
           endpoint_index == USB_ENDPOINT_AUDIO_IN ||
           endpoint_index == USB_ENDPOINT_IN ||
           endpoint_index == USB_ENDPOINT_OUT;
  }

  constexpr bool dualsense_edge_accepts_output_set_report(std::uint16_t report_value, std::uint16_t report_length) {
    return usb::hid_report_type(report_value) == usb::HID_REPORT_TYPE_OUTPUT &&
           usb::hid_report_id(report_value) == OUTPUT_REPORT_USB &&
           report_length >= OUTPUT_REPORT_COMMON_SIZE &&
           report_length <= OUTPUT_REPORT_USB_SIZE;
  }

  constexpr bool dualsense_edge_descriptor_is_interface_scoped(std::uint8_t descriptor_type) {
    return descriptor_type == usb::DESCRIPTOR_HID ||
           descriptor_type == usb::DESCRIPTOR_HID_REPORT;
  }

  inline std::span<const std::uint8_t> dualsense_edge_descriptor(std::uint8_t descriptor_type) {
    switch (descriptor_type) {
      case usb::DESCRIPTOR_DEVICE:
        return DUALSENSE_EDGE_DEVICE_DESCRIPTOR;

      case usb::DESCRIPTOR_CONFIGURATION:
        return DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR;

      case usb::DESCRIPTOR_HID:
        return {DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR.data() + DUALSENSE_EDGE_HID_DESCRIPTOR_OFFSET, 9};

      case usb::DESCRIPTOR_HID_REPORT:
        return DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR;

      default:
        return {};
    }
  }

  inline std::vector<std::uint8_t> dualsense_edge_string_descriptor(std::uint8_t descriptor_index) {
    switch (descriptor_index) {
      case 0:
        return usb::make_language_string_descriptor();

      case 1:
        return usb::make_ascii_string_descriptor(DUALSENSE_EDGE_MANUFACTURER);

      case 2:
        return usb::make_ascii_string_descriptor(DUALSENSE_EDGE_PRODUCT);

      default:
        return {};
    }
  }

  constexpr std::size_t dualsense_edge_feature_report_size(std::uint8_t report_id) {
    if ((report_id >= 0x60 && report_id <= 0x65) ||
        report_id == 0x68 ||
        (report_id >= 0x70 && report_id <= 0x7B)) {
      return 64;
    }

    switch (report_id) {
      case FEATURE_REPORT_CALIBRATION:
        return FEATURE_REPORT_CALIBRATION_SIZE;
      case 0x08:
        return 48;
      case FEATURE_REPORT_PAIRING_INFO:
        return FEATURE_REPORT_PAIRING_INFO_SIZE;
      case 0x0A:
        return 27;
      case FEATURE_REPORT_FIRMWARE_INFO:
        return FEATURE_REPORT_FIRMWARE_INFO_SIZE;
      case 0x21:
        return 5;
      case 0x22:
      case 0x80:
      case 0x81:
      case 0x83:
      case 0x84:
      case 0xE0:
      case 0xF0:
      case 0xF1:
      case 0xF4:
        return 64;
      case 0x82:
        return 10;
      case 0x85:
        return 3;
      case 0xA0:
        return 2;
      case 0xF2:
        return 53;
      case 0xF5:
        return 4;
      default:
        return 0;
    }
  }

  inline void set_le16(std::vector<std::uint8_t> &report, std::size_t offset, std::int16_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    report[offset] = static_cast<std::uint8_t>(raw & 0xFF);
    report[offset + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xFF);
  }

  inline void set_le32(std::vector<std::uint8_t> &report, std::size_t offset, std::uint32_t value) {
    report[offset] = static_cast<std::uint8_t>(value & 0xFF);
    report[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    report[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    report[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
  }

  inline std::vector<std::uint8_t> dualsense_edge_feature_report(std::uint8_t report_id) {
    const auto size = dualsense_edge_feature_report_size(report_id);
    if (size == 0) {
      return {};
    }

    std::vector<std::uint8_t> report(size);
    report[0] = report_id;

    switch (report_id) {
      case FEATURE_REPORT_CALIBRATION:
        // Synthetic but sane calibration data keeps host drivers from seeing
        // zero denominators while the virtual device reports neutral motion.
        set_le16(report, 7, 1024);
        set_le16(report, 9, -1024);
        set_le16(report, 11, 1024);
        set_le16(report, 13, -1024);
        set_le16(report, 15, 1024);
        set_le16(report, 17, -1024);
        set_le16(report, 19, 1024);
        set_le16(report, 21, 1024);
        set_le16(report, 23, 8192);
        set_le16(report, 25, -8192);
        set_le16(report, 27, 8192);
        set_le16(report, 29, -8192);
        set_le16(report, 31, 8192);
        set_le16(report, 33, -8192);
        break;

      case FEATURE_REPORT_PAIRING_INFO:
        report[1] = 0x02;
        report[2] = 0x00;
        report[3] = 0x00;
        report[4] = 0x00;
        report[5] = 0x0D;
        report[6] = 0xF2;
        break;

      case FEATURE_REPORT_FIRMWARE_INFO:
        set_le32(report, 24, 1);
        set_le32(report, 28, 1);
        set_le16(report, 44, 1);
        break;

      default:
        break;
    }

    return report;
  }

  inline control_response_t handled_control_response(std::span<const std::uint8_t> payload, std::uint16_t requested_length) {
    return control_response_t {
      .handled = true,
      .payload = usb::limited_response(payload, requested_length),
    };
  }

  inline control_response_t handled_empty_control_response() {
    return control_response_t {
      .handled = true,
      .payload = {},
    };
  }

  inline control_response_t unsupported_control_response() {
    return control_response_t {
      .handled = false,
      .payload = {},
    };
  }

  inline control_response_t dualsense_edge_control_transfer_response(const usb::setup_packet_t &setup) {
    static constexpr std::array<std::uint8_t, 2> STATUS_SELF_POWERED {1, 0};
    static constexpr std::array<std::uint8_t, 2> STATUS_INTERFACE {0, 0};
    static constexpr std::array<std::uint8_t, 2> STATUS_ENDPOINT {0, 0};
    static constexpr std::array<std::uint8_t, 1> CONFIGURATION_ACTIVE {1};
    static constexpr std::array<std::uint8_t, 1> INTERFACE_ALTERNATE_SETTING {0};
    static constexpr std::array<std::uint8_t, 1> HID_IDLE_RATE {0};
    static constexpr std::array<std::uint8_t, 1> HID_REPORT_PROTOCOL {1};

    if (usb::setup_type(setup.request_type) == usb::REQUEST_TYPE_STANDARD) {
      switch (setup.request) {
        case usb::REQUEST_GET_STATUS:
          if (!usb::setup_is_in(setup.request_type)) {
            return unsupported_control_response();
          }
          if (setup.value != 0) {
            return unsupported_control_response();
          }
          if (usb::setup_recipient(setup.request_type) == usb::RECIPIENT_DEVICE &&
              setup.index == 0) {
            return handled_control_response(STATUS_SELF_POWERED, setup.length);
          }
          if (usb::setup_recipient(setup.request_type) == usb::RECIPIENT_INTERFACE &&
              dualsense_edge_has_interface(setup.index)) {
            return handled_control_response(STATUS_INTERFACE, setup.length);
          }
          if (usb::setup_recipient(setup.request_type) == usb::RECIPIENT_ENDPOINT &&
              dualsense_edge_has_endpoint(setup.index)) {
            return handled_control_response(STATUS_ENDPOINT, setup.length);
          }
          return unsupported_control_response();

        case usb::REQUEST_CLEAR_FEATURE:
          if (!usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_ENDPOINT &&
              setup.value == usb::FEATURE_ENDPOINT_HALT &&
              setup.length == 0 &&
              dualsense_edge_has_endpoint(setup.index)) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        case usb::REQUEST_SET_ADDRESS:
          if (!usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_DEVICE &&
              setup.value <= 127 &&
              setup.index == 0 &&
              setup.length == 0) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        case usb::REQUEST_GET_DESCRIPTOR: {
          if (!usb::setup_is_in(setup.request_type)) {
            return unsupported_control_response();
          }

          const auto type = usb::descriptor_type(setup.value);
          if (type == usb::DESCRIPTOR_STRING) {
            if (usb::setup_recipient(setup.request_type) != usb::RECIPIENT_DEVICE) {
              return unsupported_control_response();
            }

            const auto descriptor = dualsense_edge_string_descriptor(usb::descriptor_index(setup.value));
            if (descriptor.empty()) {
              return unsupported_control_response();
            }

            return handled_control_response(descriptor, setup.length);
          }

          if (usb::descriptor_index(setup.value) != 0) {
            return unsupported_control_response();
          }

          if (dualsense_edge_descriptor_is_interface_scoped(type)) {
            if (usb::setup_recipient(setup.request_type) != usb::RECIPIENT_INTERFACE ||
                !dualsense_edge_targets_hid_interface(setup.index)) {
              return unsupported_control_response();
            }
          } else if (usb::setup_recipient(setup.request_type) != usb::RECIPIENT_DEVICE ||
                     setup.index != 0) {
            return unsupported_control_response();
          }

          const auto descriptor = dualsense_edge_descriptor(type);
          if (descriptor.empty()) {
            return unsupported_control_response();
          }

          return handled_control_response(descriptor, setup.length);
        }

        case usb::REQUEST_GET_CONFIGURATION:
          if (usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_DEVICE &&
              setup.value == 0 &&
              setup.index == 0) {
            return handled_control_response(CONFIGURATION_ACTIVE, setup.length);
          }
          return unsupported_control_response();

        case usb::REQUEST_SET_CONFIGURATION:
          if (!usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_DEVICE &&
              dualsense_edge_accepts_configuration(setup.value) &&
              setup.index == 0 &&
              setup.length == 0) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        case usb::REQUEST_GET_INTERFACE:
          if (usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_INTERFACE &&
              setup.value == 0 &&
              dualsense_edge_has_interface(setup.index)) {
            return handled_control_response(INTERFACE_ALTERNATE_SETTING, setup.length);
          }
          return unsupported_control_response();

        case usb::REQUEST_SET_INTERFACE:
          if (!usb::setup_is_in(setup.request_type) &&
              usb::setup_recipient(setup.request_type) == usb::RECIPIENT_INTERFACE &&
              setup.length == 0 &&
              dualsense_edge_accepts_alternate_setting(setup.index, setup.value)) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        default:
          return unsupported_control_response();
      }
    }

    if (usb::setup_type(setup.request_type) == usb::REQUEST_TYPE_CLASS) {
      if (usb::setup_recipient(setup.request_type) != usb::RECIPIENT_INTERFACE ||
          !dualsense_edge_targets_hid_interface(setup.index)) {
        return unsupported_control_response();
      }

      switch (setup.request) {
        case usb::HID_REQUEST_GET_REPORT:
          if (usb::setup_is_in(setup.request_type)) {
            if (usb::hid_report_type(setup.value) == usb::HID_REPORT_TYPE_INPUT &&
                usb::hid_report_id(setup.value) == INPUT_REPORT_USB) {
              const auto report = report_bytes(make_usb_input_report({}, 0, true));
              return handled_control_response(report, setup.length);
            }

            if (usb::hid_report_type(setup.value) == usb::HID_REPORT_TYPE_FEATURE) {
              const auto report = dualsense_edge_feature_report(usb::hid_report_id(setup.value));
              if (!report.empty()) {
                return handled_control_response(report, setup.length);
              }
            }
          }
          return unsupported_control_response();

        case usb::HID_REQUEST_GET_IDLE:
          if (usb::setup_is_in(setup.request_type) &&
              (setup.value & 0xFF00) == 0 &&
              setup.length == HID_IDLE_RATE.size()) {
            return handled_control_response(HID_IDLE_RATE, setup.length);
          }
          return unsupported_control_response();

        case usb::HID_REQUEST_GET_PROTOCOL:
          if (usb::setup_is_in(setup.request_type) &&
              setup.value == 0 &&
              setup.length == HID_REPORT_PROTOCOL.size()) {
            return handled_control_response(HID_REPORT_PROTOCOL, setup.length);
          }
          return unsupported_control_response();

        case usb::HID_REQUEST_SET_REPORT:
          if (!usb::setup_is_in(setup.request_type) &&
              dualsense_edge_accepts_output_set_report(setup.value, setup.length)) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        case usb::HID_REQUEST_SET_IDLE:
          if (!usb::setup_is_in(setup.request_type) &&
              setup.length == 0) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        case usb::HID_REQUEST_SET_PROTOCOL:
          if (!usb::setup_is_in(setup.request_type) &&
              setup.value <= 1 &&
              setup.length == 0) {
            return handled_empty_control_response();
          }
          return unsupported_control_response();

        default:
          return unsupported_control_response();
      }
    }

    return unsupported_control_response();
  }

  inline std::vector<std::uint8_t> dualsense_edge_control_response(const usb::setup_packet_t &setup) {
    return dualsense_edge_control_transfer_response(setup).payload;
  }
}  // namespace platf::dualsense
