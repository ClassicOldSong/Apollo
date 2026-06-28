/**
 * @file tests/unit/platform/test_dualsense_usbip.cpp
 * @brief Test src/platform/dualsense_usbip.h.
 */
#include "../../tests_common.h"

#include <array>
#include <cstdint>
#include <vector>

#include <src/platform/dualsense_usbip.h>

namespace {
  platf::usbip::submit_request_t submit_request(
    std::uint32_t seqnum,
    platf::usbip::direction direction,
    std::uint32_t endpoint,
    std::uint32_t length,
    std::array<std::uint8_t, 8> setup = {}
  ) {
    return platf::usbip::submit_request_t {
      .header = {
        .command_code = platf::usbip::command::cmd_submit,
        .seqnum = seqnum,
        .devid = 7,
        .transfer_direction = direction,
        .endpoint = endpoint,
      },
      .transfer_flags = 0,
      .transfer_buffer_length = length,
      .start_frame = 0,
      .number_of_packets = 0,
      .interval = 0,
      .setup = setup,
    };
  }

  std::array<std::uint8_t, 8> get_descriptor_setup(std::uint8_t descriptor_type, std::uint16_t length) {
    return {
      0x80,
      platf::usb::REQUEST_GET_DESCRIPTOR,
      0x00,
      descriptor_type,
      0x00,
      0x00,
      static_cast<std::uint8_t>(length & 0xFF),
      static_cast<std::uint8_t>((length >> 8) & 0xFF),
    };
  }

  std::vector<std::uint8_t> submit_payload(const std::vector<std::uint8_t> &reply) {
    return {reply.begin() + platf::usbip::SUBMIT_HEADER_SIZE, reply.end()};
  }
}  // namespace

TEST(DualSenseUsbIpTests, BuildsDualSenseEdgeDeviceListAndImportReplies) {
  const auto devlist = platf::dualsense_usbip::make_devlist_reply();
  const auto import = platf::dualsense_usbip::make_import_reply();

  EXPECT_EQ(platf::usbip::read_be16(devlist, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_devlist));
  EXPECT_EQ(platf::usbip::read_be32(devlist, 8), 1);
  EXPECT_EQ(platf::usbip::read_be16(devlist, 314), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(devlist[324], 0x03);

  EXPECT_EQ(platf::usbip::read_be16(import, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be16(import, 308), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(platf::usbip::read_be16(import, 310), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseUsbIpTests, BuildsControlSubmitReplyForEdgeReportDescriptor) {
  const auto request = submit_request(
    11,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE,
    get_descriptor_setup(platf::usb::DESCRIPTOR_HID_REPORT, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE)
  );

  ASSERT_TRUE(platf::dualsense_usbip::is_control_submit(request));

  const auto reply = platf::dualsense_usbip::make_control_submit_reply(request);
  const auto payload = submit_payload(reply);

  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 11);
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 7);
  EXPECT_EQ(platf::usbip::read_be32(reply, 24), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  ASSERT_EQ(payload.size(), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(payload.front(), 0x05);
  EXPECT_EQ(payload.back(), 0xC0);
}

TEST(DualSenseUsbIpTests, BuildsInterruptInSubmitReplyWithEdgeButtons) {
  auto state = platf::gamepad_state_t {
    .buttonFlags = platf::A | platf::PADDLE1,
    .lt = 64,
    .rt = 128,
    .lsX = 0,
    .lsY = 0,
    .rsX = 0,
    .rsY = 0,
  };
  const auto input_report = platf::dualsense::make_usb_input_report(state, 9, true);
  const auto request = submit_request(
    12,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  );

  ASSERT_TRUE(platf::dualsense_usbip::is_interrupt_in_submit(request));

  const auto reply = platf::dualsense_usbip::make_interrupt_in_submit_reply(request, input_report);
  const auto payload = submit_payload(reply);

  ASSERT_EQ(payload.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(payload[0], platf::dualsense::INPUT_REPORT_USB);
  EXPECT_EQ(payload[7], 9);
  EXPECT_EQ(payload[8], platf::dualsense::HAT_NONE | platf::dualsense::BUTTONS0_CROSS);
  EXPECT_EQ(payload[9], platf::dualsense::BUTTONS1_L2 | platf::dualsense::BUTTONS1_R2);
  EXPECT_EQ(payload[10], platf::dualsense::BUTTONS2_EDGE_RIGHT_PADDLE);
}

TEST(DualSenseUsbIpTests, LimitsInterruptInSubmitReplyToRequestedLength) {
  const auto input_report = platf::dualsense::make_usb_input_report({}, 0, true);
  const auto request = submit_request(
    13,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    4
  );

  const auto reply = platf::dualsense_usbip::make_interrupt_in_submit_reply(request, input_report);

  EXPECT_EQ(platf::usbip::read_be32(reply, 24), 4);
  EXPECT_EQ(submit_payload(reply).size(), 4);
}

TEST(DualSenseUsbIpTests, RecognizesAndAcknowledgesInterruptOutReports) {
  const auto request = submit_request(
    14,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_OUT,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = 0x55;

  ASSERT_TRUE(platf::dualsense_usbip::is_interrupt_out_submit(request));
  ASSERT_TRUE(platf::dualsense_usbip::is_output_report(report));

  const auto payload = platf::dualsense_usbip::output_report_payload(report);
  ASSERT_EQ(payload.size(), platf::dualsense::OUTPUT_REPORT_BODY_SIZE);
  EXPECT_EQ(payload[0], 0x55);

  const auto ack = platf::dualsense_usbip::make_interrupt_out_submit_ack(request);

  EXPECT_EQ(platf::usbip::read_be32(ack, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(ack, 4), 14);
  EXPECT_EQ(platf::usbip::read_be32(ack, 24), 0);
  EXPECT_EQ(ack.size(), platf::usbip::SUBMIT_HEADER_SIZE);
}
