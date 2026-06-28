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
  constexpr std::size_t DUALSENSE_HID_REPORT_DESCRIPTOR_SIZE = 273;
  constexpr std::size_t DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE = 389;
  constexpr std::uint8_t USB_ENDPOINT_IN = 0x84;
  constexpr std::uint8_t USB_ENDPOINT_OUT = 0x03;
  constexpr std::string_view DUALSENSE_EDGE_MANUFACTURER = "Sony Interactive Entertainment";
  constexpr std::string_view DUALSENSE_EDGE_PRODUCT = "DualSense Edge Wireless Controller";

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

  inline constexpr std::array<std::uint8_t, 41> DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR {
    0x09,  // bLength
    0x02,  // bDescriptorType: configuration
    0x29, 0x00,  // wTotalLength
    0x01,  // bNumInterfaces
    0x01,  // bConfigurationValue
    0x00,  // iConfiguration
    0xC0,  // bmAttributes: self-powered
    0xFA,  // bMaxPower: 500 mA
    0x09,  // bLength
    0x04,  // bDescriptorType: interface
    0x00,  // bInterfaceNumber
    0x00,  // bAlternateSetting
    0x02,  // bNumEndpoints
    0x03,  // bInterfaceClass: HID
    0x00,  // bInterfaceSubClass
    0x00,  // bInterfaceProtocol
    0x00,  // iInterface
    0x09,  // bLength
    0x21,  // bDescriptorType: HID
    0x11, 0x01,  // bcdHID 1.11
    0x00,  // bCountryCode
    0x01,  // bNumDescriptors
    0x22,  // bDescriptorType: report
    0x85, 0x01,  // wDescriptorLength: DualSense Edge report descriptor
    0x07,  // bLength
    0x05,  // bDescriptorType: endpoint
    USB_ENDPOINT_IN,
    0x03,  // bmAttributes: interrupt
    0x40, 0x00,  // wMaxPacketSize
    0x06,  // bInterval
    0x07,  // bLength
    0x05,  // bDescriptorType: endpoint
    USB_ENDPOINT_OUT,
    0x03,  // bmAttributes: interrupt
    0x40, 0x00,  // wMaxPacketSize
    0x06,  // bInterval
  };

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
      // SDL exposes DualSense Edge raw buttons b20, b19, b18, b17 as PADDLE1
      // through PADDLE4. Preserve that order when packing the Edge-only byte.
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

    return report;
  }

  constexpr usb_input_report_t make_usb_input_report(const gamepad_state_t &gamepad_state, std::uint8_t sequence_number, bool dualsense_edge) {
    return usb_input_report_t {
      .report_id = INPUT_REPORT_USB,
      .body = make_input_report(gamepad_state, sequence_number, dualsense_edge),
    };
  }

  inline std::span<const std::uint8_t> dualsense_edge_descriptor(std::uint8_t descriptor_type) {
    switch (descriptor_type) {
      case usb::DESCRIPTOR_DEVICE:
        return DUALSENSE_EDGE_DEVICE_DESCRIPTOR;

      case usb::DESCRIPTOR_CONFIGURATION:
        return DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR;

      case usb::DESCRIPTOR_HID:
        return {DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR.data() + 18, 9};

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

  inline std::vector<std::uint8_t> dualsense_edge_control_response(const usb::setup_packet_t &setup) {
    if (usb::setup_type(setup.request_type) != usb::REQUEST_TYPE_STANDARD ||
        setup.request != usb::REQUEST_GET_DESCRIPTOR) {
      return {};
    }

    const auto type = usb::descriptor_type(setup.value);
    if (type == usb::DESCRIPTOR_STRING) {
      return usb::limited_response(dualsense_edge_string_descriptor(usb::descriptor_index(setup.value)), setup.length);
    }

    return usb::limited_response(dualsense_edge_descriptor(type), setup.length);
  }
}  // namespace platf::dualsense
