/**
 * @file src/platform/usb.h
 * @brief Shared helpers for USB control requests and descriptors.
 */
#pragma once

// standard includes
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace platf::usb {
  constexpr std::uint8_t REQUEST_DIRECTION_IN = 0x80;
  constexpr std::uint8_t REQUEST_RECIPIENT_MASK = 0x1F;

  constexpr std::uint8_t REQUEST_TYPE_STANDARD = 0;
  constexpr std::uint8_t REQUEST_TYPE_CLASS = 1;

  constexpr std::uint8_t RECIPIENT_DEVICE = 0;
  constexpr std::uint8_t RECIPIENT_INTERFACE = 1;

  constexpr std::uint8_t REQUEST_GET_STATUS = 0x00;
  constexpr std::uint8_t REQUEST_GET_DESCRIPTOR = 0x06;
  constexpr std::uint8_t REQUEST_GET_CONFIGURATION = 0x08;
  constexpr std::uint8_t REQUEST_SET_CONFIGURATION = 0x09;
  constexpr std::uint8_t REQUEST_GET_INTERFACE = 0x0A;
  constexpr std::uint8_t REQUEST_SET_INTERFACE = 0x0B;

  constexpr std::uint8_t HID_REQUEST_GET_IDLE = 0x02;
  constexpr std::uint8_t HID_REQUEST_GET_PROTOCOL = 0x03;
  constexpr std::uint8_t HID_REQUEST_SET_REPORT = 0x09;
  constexpr std::uint8_t HID_REQUEST_SET_IDLE = 0x0A;
  constexpr std::uint8_t HID_REQUEST_SET_PROTOCOL = 0x0B;

  constexpr std::uint8_t DESCRIPTOR_DEVICE = 0x01;
  constexpr std::uint8_t DESCRIPTOR_CONFIGURATION = 0x02;
  constexpr std::uint8_t DESCRIPTOR_STRING = 0x03;
  constexpr std::uint8_t DESCRIPTOR_HID = 0x21;
  constexpr std::uint8_t DESCRIPTOR_HID_REPORT = 0x22;

  constexpr std::uint16_t LANGID_EN_US = 0x0409;

  struct setup_packet_t {
    std::uint8_t request_type;
    std::uint8_t request;
    std::uint16_t value;
    std::uint16_t index;
    std::uint16_t length;
  };

  inline std::uint16_t read_le16(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8);
  }

  inline setup_packet_t parse_setup_packet(std::span<const std::uint8_t> setup) {
    return setup_packet_t {
      .request_type = setup[0],
      .request = setup[1],
      .value = read_le16(setup, 2),
      .index = read_le16(setup, 4),
      .length = read_le16(setup, 6),
    };
  }

  constexpr std::uint8_t setup_type(std::uint8_t request_type) {
    return (request_type >> 5) & 0x03;
  }

  constexpr bool setup_is_in(std::uint8_t request_type) {
    return request_type & REQUEST_DIRECTION_IN;
  }

  constexpr std::uint8_t setup_recipient(std::uint8_t request_type) {
    return request_type & REQUEST_RECIPIENT_MASK;
  }

  constexpr std::uint8_t descriptor_type(std::uint16_t value) {
    return static_cast<std::uint8_t>(value >> 8);
  }

  constexpr std::uint8_t descriptor_index(std::uint16_t value) {
    return static_cast<std::uint8_t>(value & 0xFF);
  }

  inline std::vector<std::uint8_t> make_language_string_descriptor(std::uint16_t langid = LANGID_EN_US) {
    return {
      0x04,
      DESCRIPTOR_STRING,
      static_cast<std::uint8_t>(langid & 0xFF),
      static_cast<std::uint8_t>((langid >> 8) & 0xFF),
    };
  }

  inline std::vector<std::uint8_t> make_ascii_string_descriptor(std::string_view text) {
    const auto copy_len = std::min<std::size_t>(text.size(), 126);
    std::vector<std::uint8_t> descriptor;
    descriptor.reserve(2 + copy_len * 2);

    descriptor.push_back(static_cast<std::uint8_t>(2 + copy_len * 2));
    descriptor.push_back(DESCRIPTOR_STRING);
    for (std::size_t i = 0; i < copy_len; ++i) {
      descriptor.push_back(static_cast<std::uint8_t>(text[i]));
      descriptor.push_back(0);
    }

    return descriptor;
  }

  inline std::vector<std::uint8_t> limited_response(std::span<const std::uint8_t> payload, std::uint16_t requested_length) {
    const auto size = std::min<std::size_t>(payload.size(), requested_length);
    return {payload.begin(), payload.begin() + size};
  }
}  // namespace platf::usb
