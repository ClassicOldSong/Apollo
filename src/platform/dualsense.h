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

// local includes
#include "src/platform/common.h"

namespace platf::dualsense {
  constexpr std::uint8_t INPUT_REPORT_USB = 0x01;
  constexpr std::size_t INPUT_REPORT_USB_SIZE = 64;

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

  static_assert(sizeof(input_report_t) == INPUT_REPORT_USB_SIZE - 1);

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
}  // namespace platf::dualsense
