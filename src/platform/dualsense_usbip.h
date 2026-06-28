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
#include <deque>
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
  constexpr std::size_t MAX_PENDING_INTERRUPT_IN = 32;
  constexpr std::uint32_t MAX_OUT_PAYLOAD_SIZE = dualsense::OUTPUT_REPORT_USB_SIZE;
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

  struct submit_result_t {
    std::vector<std::uint8_t> reply;
    bool has_output_report;
    dualsense::output_report_state_t output_report;
  };

  struct unlink_result_t {
    std::vector<std::uint8_t> reply;
    bool removed;
  };

  inline std::vector<std::uint8_t> make_devlist_reply() {
    return usbip::make_devlist_reply(DEVICE, INTERFACES);
  }

  inline std::vector<std::uint8_t> make_import_reply() {
    return usbip::make_import_reply(DEVICE);
  }

  inline std::vector<std::uint8_t> handle_op_request(std::span<const std::uint8_t> request) {
    if (!usbip::has_bytes(request, 0, usbip::OP_COMMON_SIZE)) {
      return {};
    }

    const auto common = usbip::parse_op_common(request);

    if (common.version != usbip::VERSION) {
      return {};
    }

    switch (common.code) {
      case usbip::op_code::req_devlist:
        return make_devlist_reply();

      case usbip::op_code::req_import:
        if (!usbip::has_bytes(request, 0, usbip::OP_COMMON_SIZE + usbip::USB_DEVICE_BUSID_SIZE)) {
          return {};
        }

        if (usbip::parse_import_request(request).busid == BUSID) {
          return make_import_reply();
        }
        return usbip::make_import_error_reply(1);

      default:
        return {};
    }
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

  inline bool is_valid_out_payload_length(const usbip::submit_request_t &request) {
    return request.header.transfer_direction != usbip::direction::out ||
           request.transfer_buffer_length <= MAX_OUT_PAYLOAD_SIZE;
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

  inline bool is_output_set_report(const usbip::submit_request_t &request) {
    const auto setup = usb::parse_setup_packet(request.setup);
    return is_control_submit(request) &&
           !usb::setup_is_in(setup.request_type) &&
           usb::setup_type(setup.request_type) == usb::REQUEST_TYPE_CLASS &&
           usb::setup_recipient(setup.request_type) == usb::RECIPIENT_INTERFACE &&
           setup.request == usb::HID_REQUEST_SET_REPORT &&
           usb::hid_report_type(setup.value) == usb::HID_REPORT_TYPE_OUTPUT &&
           usb::hid_report_id(setup.value) == dualsense::OUTPUT_REPORT_USB;
  }

  inline dualsense::output_report_state_t parse_output_report_body(std::span<const std::uint8_t> payload) {
    std::array<std::uint8_t, dualsense::OUTPUT_REPORT_USB_SIZE> report {};
    report[0] = dualsense::OUTPUT_REPORT_USB;
    std::copy_n(
      payload.begin(),
      std::min(payload.size(), dualsense::OUTPUT_REPORT_BODY_SIZE),
      report.begin() + 1
    );
    return dualsense::parse_output_report(report);
  }

  inline dualsense::output_report_state_t parse_output_set_report(
    const usbip::submit_request_t &request,
    std::span<const std::uint8_t> payload
  ) {
    if (!is_output_set_report(request)) {
      return {};
    }

    if (payload.size() == dualsense::OUTPUT_REPORT_BODY_SIZE ||
        payload.size() == dualsense::OUTPUT_REPORT_COMMON_SIZE) {
      return parse_output_report_body(payload);
    }

    if (is_output_report(payload)) {
      return dualsense::parse_output_report(payload);
    }

    if (payload.size() >= dualsense::OUTPUT_REPORT_COMMON_SIZE) {
      return parse_output_report_body(payload);
    }

    return {};
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

  class device_t {
  public:
    void update_input(const gamepad_state_t &state) {
      input_state_ = state;
    }

    const gamepad_state_t &input_state() const {
      return input_state_;
    }

    submit_result_t handle_submit(const usbip::submit_request_t &request, std::span<const std::uint8_t> out_payload = {}) {
      if (is_control_submit(request)) {
        const auto output = parse_output_set_report(request, out_payload);
        return {
          .reply = make_control_submit_reply(request),
          .has_output_report = output.valid,
          .output_report = output,
        };
      }

      if (is_interrupt_in_submit(request)) {
        const auto report = dualsense::make_usb_input_report(input_state_, sequence_number_++, true);
        return {
          .reply = make_interrupt_in_submit_reply(request, report),
          .has_output_report = false,
          .output_report = {},
        };
      }

      if (is_interrupt_out_submit(request)) {
        const auto output = dualsense::parse_output_report(out_payload);
        return {
          .reply = make_interrupt_out_submit_ack(request),
          .has_output_report = output.valid,
          .output_report = output,
        };
      }

      return {
        .reply = usbip::make_ret_submit(
          request.header.seqnum,
          request.header.devid,
          request.header.transfer_direction,
          request.header.endpoint,
          -1
        ),
        .has_output_report = false,
        .output_report = {},
      };
    }

  private:
    gamepad_state_t input_state_ {};
    std::uint8_t sequence_number_ {};
  };

  class session_t {
  public:
    std::vector<std::uint8_t> handle_op_request(std::span<const std::uint8_t> request) {
      return dualsense_usbip::handle_op_request(request);
    }

    void update_input(const gamepad_state_t &state) {
      device_.update_input(state);
    }

    const gamepad_state_t &input_state() const {
      return device_.input_state();
    }

    submit_result_t handle_submit(const usbip::submit_request_t &request, std::span<const std::uint8_t> out_payload = {}) {
      if (is_interrupt_in_submit(request)) {
        if (pending_interrupt_in_.size() >= MAX_PENDING_INTERRUPT_IN) {
          return device_.handle_submit(request);
        }

        pending_interrupt_in_.push_back(request);
        return {};
      }

      return device_.handle_submit(request, out_payload);
    }

    unlink_result_t handle_unlink(const usbip::unlink_request_t &request) {
      bool removed = false;
      for (auto it = pending_interrupt_in_.begin(); it != pending_interrupt_in_.end();) {
        if (it->header.seqnum == request.unlink_seqnum) {
          it = pending_interrupt_in_.erase(it);
          removed = true;
        } else {
          ++it;
        }
      }

      return {
        .reply = usbip::make_ret_unlink(
          request.header.seqnum,
          request.header.devid,
          removed ? 0 : -1
        ),
        .removed = removed,
      };
    }

    bool has_pending_interrupt_in() const {
      return !pending_interrupt_in_.empty();
    }

    std::size_t pending_interrupt_in_count() const {
      return pending_interrupt_in_.size();
    }

    std::vector<std::uint8_t> take_next_interrupt_in_reply() {
      if (pending_interrupt_in_.empty()) {
        return {};
      }

      const auto request = pending_interrupt_in_.front();
      pending_interrupt_in_.pop_front();
      return device_.handle_submit(request).reply;
    }

  private:
    device_t device_;
    std::deque<usbip::submit_request_t> pending_interrupt_in_;
  };
}  // namespace platf::dualsense_usbip
