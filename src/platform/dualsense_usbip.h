/**
 * @file src/platform/dualsense_usbip.h
 * @brief USB/IP adapter helpers for a virtual DualSense Edge device.
 */
#pragma once

// standard includes
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// local includes
#include "src/platform/dualsense.h"
#include "src/platform/usb.h"
#include "src/platform/usbip.h"

namespace platf::dualsense_usbip {
  constexpr std::uint32_t ENDPOINT_CONTROL = 0;
  constexpr std::uint32_t ENDPOINT_INTERRUPT_OUT = dualsense::USB_ENDPOINT_OUT & 0x0F;
  constexpr std::uint32_t ENDPOINT_INTERRUPT_IN = dualsense::USB_ENDPOINT_IN & 0x0F;
  constexpr std::string_view DEVICE_PATH = "/sys/devices/platform/apollo-dualsense-edge";
  constexpr std::string_view BUSID = "1-1";

  inline constexpr usbip::device_identity_t DEVICE {
    .path = DEVICE_PATH,
    .busid = BUSID,
    .busnum = 1,
    .devnum = 2,
    .speed = usbip::device_speed::full,
    .vendor_id = dualsense::VENDOR_ID_SONY,
    .product_id = dualsense::PRODUCT_ID_DUALSENSE_EDGE,
    .bcd_device = 0x0100,
    .device_class = 0,
    .device_subclass = 0,
    .device_protocol = 0,
    .configuration_value = 1,
    .configuration_count = 1,
    .interface_count = 1,
  };

  inline constexpr std::array<usbip::interface_identity_t, 1> INTERFACES {
    usbip::interface_identity_t {
      .interface_class = 0x03,
      .interface_subclass = 0x00,
      .interface_protocol = 0x00,
    },
  };

  inline std::vector<std::uint8_t> make_devlist_reply() {
    return usbip::make_devlist_reply(DEVICE, INTERFACES);
  }

  inline std::vector<std::uint8_t> make_import_reply() {
    return usbip::make_import_reply(DEVICE);
  }

  inline std::vector<std::uint8_t> limited_payload(std::span<const std::uint8_t> payload, std::uint32_t requested_length) {
    const auto size = std::min<std::size_t>(payload.size(), requested_length);
    return {payload.begin(), payload.begin() + size};
  }

  inline std::span<const std::uint8_t> input_report_bytes(const dualsense::usb_input_report_t &report) {
    return {
      reinterpret_cast<const std::uint8_t *>(&report),
      sizeof(report),
    };
  }

  inline bool is_control_submit(const usbip::submit_request_t &request) {
    return request.header.command_code == usbip::command::cmd_submit &&
           request.header.endpoint == ENDPOINT_CONTROL;
  }

  inline bool is_interrupt_in_submit(const usbip::submit_request_t &request) {
    return request.header.command_code == usbip::command::cmd_submit &&
           request.header.transfer_direction == usbip::direction::in &&
           request.header.endpoint == ENDPOINT_INTERRUPT_IN;
  }

  inline bool is_interrupt_out_submit(const usbip::submit_request_t &request) {
    return request.header.command_code == usbip::command::cmd_submit &&
           request.header.transfer_direction == usbip::direction::out &&
           request.header.endpoint == ENDPOINT_INTERRUPT_OUT;
  }

  inline std::vector<std::uint8_t> make_control_submit_reply(const usbip::submit_request_t &request) {
    const auto setup = usb::parse_setup_packet(request.setup);
    const auto payload = dualsense::dualsense_edge_control_response(setup);

    return usbip::make_ret_submit(
      request.header.seqnum,
      request.header.devid,
      request.header.transfer_direction,
      request.header.endpoint,
      0,
      payload
    );
  }

  inline std::vector<std::uint8_t> make_interrupt_in_submit_reply(
    const usbip::submit_request_t &request,
    const dualsense::usb_input_report_t &input_report
  ) {
    const auto payload = limited_payload(input_report_bytes(input_report), request.transfer_buffer_length);

    return usbip::make_ret_submit(
      request.header.seqnum,
      request.header.devid,
      request.header.transfer_direction,
      request.header.endpoint,
      0,
      payload
    );
  }

  inline bool is_output_report(std::span<const std::uint8_t> payload) {
    return payload.size() >= dualsense::OUTPUT_REPORT_USB_MIN_SIZE &&
           payload.front() == dualsense::OUTPUT_REPORT_USB;
  }

  inline std::span<const std::uint8_t> output_report_payload(std::span<const std::uint8_t> payload) {
    if (!is_output_report(payload)) {
      return {};
    }

    return payload.subspan(1);
  }

  inline std::vector<std::uint8_t> make_interrupt_out_submit_ack(const usbip::submit_request_t &request) {
    return usbip::make_ret_submit(
      request.header.seqnum,
      request.header.devid,
      request.header.transfer_direction,
      request.header.endpoint,
      0
    );
  }
}  // namespace platf::dualsense_usbip
