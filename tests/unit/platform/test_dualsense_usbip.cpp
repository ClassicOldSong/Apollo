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
        .devid = platf::dualsense_usbip::DEVID,
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

  platf::usbip::unlink_request_t unlink_request(std::uint32_t seqnum, std::uint32_t unlink_seqnum) {
    return platf::usbip::unlink_request_t {
      .header = {
        .command_code = platf::usbip::command::cmd_unlink,
        .seqnum = seqnum,
        .devid = platf::dualsense_usbip::DEVID,
        .transfer_direction = platf::usbip::direction::out,
        .endpoint = 0,
      },
      .unlink_seqnum = unlink_seqnum,
    };
  }

  std::array<std::uint8_t, 8> get_descriptor_setup(
    std::uint8_t descriptor_type,
    std::uint16_t length,
    std::uint16_t interface_index = platf::dualsense::DUALSENSE_EDGE_INTERFACE_HID
  ) {
    const auto request_type = platf::dualsense::dualsense_edge_descriptor_is_interface_scoped(descriptor_type) ?
                                static_cast<std::uint8_t>(platf::usb::REQUEST_DIRECTION_IN | platf::usb::RECIPIENT_INTERFACE) :
                                platf::usb::REQUEST_DIRECTION_IN;

    return {
      request_type,
      platf::usb::REQUEST_GET_DESCRIPTOR,
      0x00,
      descriptor_type,
      static_cast<std::uint8_t>(interface_index & 0xFF),
      static_cast<std::uint8_t>((interface_index >> 8) & 0xFF),
      static_cast<std::uint8_t>(length & 0xFF),
      static_cast<std::uint8_t>((length >> 8) & 0xFF),
    };
  }

  std::array<std::uint8_t, 8> get_report_setup(
    std::uint8_t report_type,
    std::uint8_t report_id,
    std::uint16_t length,
    std::uint16_t interface_index = platf::dualsense::DUALSENSE_EDGE_INTERFACE_HID
  ) {
    return {
      static_cast<std::uint8_t>(platf::usb::REQUEST_DIRECTION_IN |
                                (platf::usb::REQUEST_TYPE_CLASS << 5) |
                                platf::usb::RECIPIENT_INTERFACE),
      platf::usb::HID_REQUEST_GET_REPORT,
      report_id,
      report_type,
      static_cast<std::uint8_t>(interface_index & 0xFF),
      static_cast<std::uint8_t>((interface_index >> 8) & 0xFF),
      static_cast<std::uint8_t>(length & 0xFF),
      static_cast<std::uint8_t>((length >> 8) & 0xFF),
    };
  }

  std::array<std::uint8_t, 8> set_report_setup(
    std::uint8_t report_type,
    std::uint8_t report_id,
    std::uint16_t length,
    std::uint16_t interface_index = platf::dualsense::DUALSENSE_EDGE_INTERFACE_HID
  ) {
    return {
      static_cast<std::uint8_t>((platf::usb::REQUEST_TYPE_CLASS << 5) |
                                platf::usb::RECIPIENT_INTERFACE),
      platf::usb::HID_REQUEST_SET_REPORT,
      report_id,
      report_type,
      static_cast<std::uint8_t>(interface_index & 0xFF),
      static_cast<std::uint8_t>((interface_index >> 8) & 0xFF),
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
  EXPECT_EQ(platf::usbip::read_be32(devlist, 308), static_cast<std::uint32_t>(platf::usbip::device_speed::high));
  EXPECT_EQ(platf::usbip::read_be16(devlist, 314), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(devlist[323], 4);
  EXPECT_EQ(devlist[324], 0x01);
  EXPECT_EQ(devlist[336], 0x03);

  EXPECT_EQ(platf::usbip::read_be16(import, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be32(import, 304), static_cast<std::uint32_t>(platf::usbip::device_speed::high));
  EXPECT_EQ(platf::usbip::read_be16(import, 308), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(platf::usbip::read_be16(import, 310), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseUsbIpTests, HandlesUsbIpDeviceListRequest) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_be16(request, platf::usbip::VERSION);
  platf::usbip::append_be16(request, static_cast<std::uint16_t>(platf::usbip::op_code::req_devlist));
  platf::usbip::append_be32(request, 0);

  const auto reply = platf::dualsense_usbip::handle_op_request(request);

  ASSERT_FALSE(reply.empty());
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_devlist));
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 1);
  EXPECT_EQ(platf::usbip::read_be16(reply, 314), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseUsbIpTests, HandlesUsbIpImportRequestForEdgeBusId) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_be16(request, platf::usbip::VERSION);
  platf::usbip::append_be16(request, static_cast<std::uint16_t>(platf::usbip::op_code::req_import));
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_fixed_string(request, platf::dualsense_usbip::BUSID, platf::usbip::USB_DEVICE_BUSID_SIZE);

  const auto reply = platf::dualsense_usbip::handle_op_request(request);

  ASSERT_FALSE(reply.empty());
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be16(reply, 308), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(platf::usbip::read_be16(reply, 310), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseUsbIpTests, RejectsUsbIpImportRequestForDifferentBusId) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_be16(request, platf::usbip::VERSION);
  platf::usbip::append_be16(request, static_cast<std::uint16_t>(platf::usbip::op_code::req_import));
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_fixed_string(request, "2-1", platf::usbip::USB_DEVICE_BUSID_SIZE);

  const auto reply = platf::dualsense_usbip::handle_op_request(request);

  ASSERT_EQ(reply.size(), platf::usbip::OP_COMMON_SIZE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 1);
}

TEST(DualSenseUsbIpTests, RejectsShortOpRequests) {
  std::vector<std::uint8_t> short_common {
    0x01,
    0x11,
    0x80,
  };
  EXPECT_TRUE(platf::dualsense_usbip::handle_op_request(short_common).empty());

  std::vector<std::uint8_t> short_import;
  platf::usbip::append_be16(short_import, platf::usbip::VERSION);
  platf::usbip::append_be16(short_import, static_cast<std::uint16_t>(platf::usbip::op_code::req_import));
  platf::usbip::append_be32(short_import, 0);
  EXPECT_TRUE(platf::dualsense_usbip::handle_op_request(short_import).empty());
}

TEST(DualSenseUsbIpTests, RejectsUnsupportedUsbIpOpRequests) {
  std::vector<std::uint8_t> wrong_version;
  platf::usbip::append_be16(wrong_version, 0x0110);
  platf::usbip::append_be16(wrong_version, static_cast<std::uint16_t>(platf::usbip::op_code::req_devlist));
  platf::usbip::append_be32(wrong_version, 0);
  EXPECT_TRUE(platf::dualsense_usbip::handle_op_request(wrong_version).empty());

  std::vector<std::uint8_t> unknown_op;
  platf::usbip::append_be16(unknown_op, platf::usbip::VERSION);
  platf::usbip::append_be16(unknown_op, 0x8000);
  platf::usbip::append_be32(unknown_op, 0);
  EXPECT_TRUE(platf::dualsense_usbip::handle_op_request(unknown_op).empty());
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
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), platf::dualsense_usbip::DEVID);
  EXPECT_EQ(platf::usbip::read_be32(reply, 20), 0);
  EXPECT_EQ(platf::usbip::read_be32(reply, 24), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  ASSERT_EQ(payload.size(), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(payload.front(), 0x05);
  EXPECT_EQ(payload.back(), 0xC0);
}

TEST(DualSenseUsbIpTests, BuildsControlSubmitReplyForEdgeFeatureReport) {
  const auto request = submit_request(
    12,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE,
    get_report_setup(
      platf::usb::HID_REPORT_TYPE_FEATURE,
      platf::dualsense::FEATURE_REPORT_CALIBRATION,
      platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE
    )
  );

  ASSERT_TRUE(platf::dualsense_usbip::is_control_submit(request));

  const auto reply = platf::dualsense_usbip::make_control_submit_reply(request);
  const auto payload = submit_payload(reply);

  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 12);
  EXPECT_EQ(platf::usbip::read_be32(reply, 24), platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE);
  ASSERT_EQ(payload.size(), platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE);
  EXPECT_EQ(payload[0], platf::dualsense::FEATURE_REPORT_CALIBRATION);
  EXPECT_EQ(payload[8], 0x04);
  EXPECT_EQ(payload[10], 0xFC);
}

TEST(DualSenseUsbIpTests, BuildsControlSubmitReplyForEdgeInputReport) {
  const auto request = submit_request(
    13,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::INPUT_REPORT_USB_SIZE,
    get_report_setup(
      platf::usb::HID_REPORT_TYPE_INPUT,
      platf::dualsense::INPUT_REPORT_USB,
      platf::dualsense::INPUT_REPORT_USB_SIZE
    )
  );

  ASSERT_TRUE(platf::dualsense_usbip::is_control_submit(request));

  const auto reply = platf::dualsense_usbip::make_control_submit_reply(request);
  const auto payload = submit_payload(reply);

  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 13);
  EXPECT_EQ(platf::usbip::read_be32(reply, 24), platf::dualsense::INPUT_REPORT_USB_SIZE);
  ASSERT_EQ(payload.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(payload[0], platf::dualsense::INPUT_REPORT_USB);
  EXPECT_EQ(payload[8], platf::dualsense::HAT_NONE);
  EXPECT_EQ(payload[33], 0x80);
  EXPECT_EQ(payload[37], 0x80);
}

TEST(DualSenseUsbIpTests, ReturnsControlStallForUnsupportedEdgeControlRequests) {
  const auto wrong_interface = platf::dualsense_usbip::make_control_submit_reply(submit_request(
    14,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    9,
    get_descriptor_setup(
      platf::usb::DESCRIPTOR_HID,
      9,
      platf::dualsense::DUALSENSE_EDGE_INTERFACE_AUDIO_CONTROL
    )
  ));

  EXPECT_EQ(platf::usbip::read_be32(wrong_interface, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(wrong_interface, 4), 14);
  EXPECT_EQ(platf::usbip::read_be32(wrong_interface, 20), static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS));
  EXPECT_EQ(platf::usbip::read_be32(wrong_interface, 24), 0);
  EXPECT_TRUE(submit_payload(wrong_interface).empty());

  const auto unknown_feature = platf::dualsense_usbip::make_control_submit_reply(submit_request(
    15,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    64,
    get_report_setup(platf::usb::HID_REPORT_TYPE_FEATURE, 0x99, 64)
  ));

  EXPECT_EQ(platf::usbip::read_be32(unknown_feature, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(unknown_feature, 4), 15);
  EXPECT_EQ(platf::usbip::read_be32(unknown_feature, 20), static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS));
  EXPECT_EQ(platf::usbip::read_be32(unknown_feature, 24), 0);
  EXPECT_TRUE(submit_payload(unknown_feature).empty());
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

TEST(DualSenseUsbIpTests, RecognizesMinimumCommonOutputReport) {
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_MIN_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;

  EXPECT_TRUE(platf::dualsense_usbip::is_output_report(report));
  EXPECT_EQ(
    platf::dualsense_usbip::output_report_payload(report).size(),
    platf::dualsense::OUTPUT_REPORT_COMMON_SIZE
  );
}

TEST(DualSenseUsbIpTests, BoundsOutPayloadLength) {
  EXPECT_TRUE(platf::dualsense_usbip::is_valid_out_payload_length(submit_request(
    21,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_OUT,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE
  )));

  EXPECT_FALSE(platf::dualsense_usbip::is_valid_out_payload_length(submit_request(
    22,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_OUT,
    platf::dualsense_usbip::MAX_OUT_PAYLOAD_SIZE + 1
  )));

  EXPECT_TRUE(platf::dualsense_usbip::is_valid_out_payload_length(submit_request(
    23,
    platf::usbip::direction::out,
    1,
    392
  )));

  EXPECT_FALSE(platf::dualsense_usbip::is_valid_out_payload_length(submit_request(
    24,
    platf::usbip::direction::out,
    1,
    platf::dualsense_usbip::MAX_UNSUPPORTED_OUT_PAYLOAD_SIZE + 1
  )));

  EXPECT_TRUE(platf::dualsense_usbip::is_valid_out_payload_length(submit_request(
    25,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense_usbip::MAX_OUT_PAYLOAD_SIZE + 1
  )));
}

TEST(DualSenseUsbIpTests, DeviceHandlesInterruptInSubmitsWithUpdatedInputState) {
  platf::dualsense_usbip::device_t device;
  device.update_input(platf::gamepad_state_t {
    .buttonFlags = platf::PADDLE4,
    .lt = 0,
    .rt = 0,
    .lsX = 0,
    .lsY = 0,
    .rsX = 0,
    .rsY = 0,
  });

  const auto first = device.handle_submit(submit_request(
    20,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));
  const auto second = device.handle_submit(submit_request(
    21,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));

  const auto first_payload = submit_payload(first.reply);
  const auto second_payload = submit_payload(second.reply);

  ASSERT_EQ(first_payload.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  ASSERT_EQ(second_payload.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(first_payload[7], 0);
  EXPECT_EQ(second_payload[7], 1);
  EXPECT_EQ(first_payload[10], platf::dualsense::BUTTONS2_EDGE_FN_LEFT);
  EXPECT_FALSE(first.has_output_report);
}

TEST(DualSenseUsbIpTests, DeviceHandlesInterruptOutSubmitWithParsedFeedback) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    22,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_OUT,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[2] = platf::dualsense::OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
  report[3] = 0x20;
  report[4] = 0x40;
  report[45] = 0xAA;
  report[46] = 0xBB;
  report[47] = 0xCC;

  const auto result = device.handle_submit(request, report);

  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_TRUE(result.has_output_report);
  EXPECT_TRUE(result.output_report.has_rumble);
  EXPECT_EQ(result.output_report.motor_left, 0x40);
  EXPECT_EQ(result.output_report.motor_right, 0x20);
  EXPECT_TRUE(result.output_report.has_lightbar);
  EXPECT_EQ(result.output_report.lightbar_red, 0xAA);
  EXPECT_EQ(result.output_report.lightbar_green, 0xBB);
  EXPECT_EQ(result.output_report.lightbar_blue, 0xCC);
}

TEST(DualSenseUsbIpTests, DeviceRejectsControlSubmitWithMismatchedSetupDirection) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    23,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    0,
    get_descriptor_setup(
      platf::usb::DESCRIPTOR_HID_REPORT,
      platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE
    )
  );

  const auto result = device.handle_submit(request);

  EXPECT_FALSE(result.has_output_report);
  EXPECT_TRUE(submit_payload(result.reply).empty());
  EXPECT_EQ(
    platf::usbip::read_be32(result.reply, 20),
    static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS)
  );
}

TEST(DualSenseUsbIpTests, DeviceRejectsControlInputWithMismatchedSetupLength) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    30,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    4,
    get_descriptor_setup(
      platf::usb::DESCRIPTOR_HID_REPORT,
      platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE
    )
  );

  const auto result = device.handle_submit(request);

  EXPECT_FALSE(result.has_output_report);
  EXPECT_TRUE(submit_payload(result.reply).empty());
  EXPECT_EQ(
    platf::usbip::read_be32(result.reply, 20),
    static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS)
  );
}

TEST(DualSenseUsbIpTests, DeviceHandlesControlOutputSetReportWithParsedFeedback) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    24,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_USB_SIZE
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[3] = 0x10;
  report[4] = 0x30;

  const auto result = device.handle_submit(request, report);

  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 24);
  EXPECT_TRUE(result.has_output_report);
  EXPECT_TRUE(result.output_report.has_rumble);
  EXPECT_EQ(result.output_report.motor_left, 0x30);
  EXPECT_EQ(result.output_report.motor_right, 0x10);
}

TEST(DualSenseUsbIpTests, DeviceHandlesControlOutputSetReportWithoutReportIdPrefix) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    25,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_BODY_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_BODY_SIZE
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_BODY_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[2] = 0x20;
  report[3] = 0x40;

  const auto result = device.handle_submit(request, report);

  EXPECT_TRUE(result.has_output_report);
  EXPECT_TRUE(result.output_report.has_rumble);
  EXPECT_EQ(result.output_report.motor_left, 0x40);
  EXPECT_EQ(result.output_report.motor_right, 0x20);
}

TEST(DualSenseUsbIpTests, DeviceHandlesCommonLengthControlOutputSetReport) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    26,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_COMMON_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_COMMON_SIZE
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_COMMON_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[2] = 0x20;
  report[3] = 0x40;

  const auto result = device.handle_submit(request, report);

  EXPECT_FALSE(result.reply.empty());
  EXPECT_TRUE(result.has_output_report);
  EXPECT_TRUE(result.output_report.has_rumble);
  EXPECT_EQ(result.output_report.motor_left, 0x40);
  EXPECT_EQ(result.output_report.motor_right, 0x20);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 26);
}

TEST(DualSenseUsbIpTests, DeviceRejectsControlOutputSetReportWithInvalidSetupLength) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    27,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_COMMON_SIZE - 1
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[3] = 0x10;
  report[4] = 0x30;

  const auto result = device.handle_submit(request, report);

  EXPECT_FALSE(result.has_output_report);
  EXPECT_EQ(
    platf::usbip::read_be32(result.reply, 20),
    static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS)
  );
}

TEST(DualSenseUsbIpTests, DeviceRejectsControlOutputSetReportWithMismatchedPayloadLength) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    28,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_COMMON_SIZE
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[3] = 0x10;
  report[4] = 0x30;

  const auto result = device.handle_submit(request, report);

  EXPECT_FALSE(result.has_output_report);
  EXPECT_EQ(
    platf::usbip::read_be32(result.reply, 20),
    static_cast<std::uint32_t>(platf::dualsense_usbip::CONTROL_STALL_STATUS)
  );
}

TEST(DualSenseUsbIpTests, DeviceIgnoresControlOutputSetReportForNonHidInterface) {
  platf::dualsense_usbip::device_t device;
  const auto request = submit_request(
    29,
    platf::usbip::direction::out,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::OUTPUT_REPORT_USB_SIZE,
    set_report_setup(
      platf::usb::HID_REPORT_TYPE_OUTPUT,
      platf::dualsense::OUTPUT_REPORT_USB,
      platf::dualsense::OUTPUT_REPORT_USB_SIZE,
      platf::dualsense::DUALSENSE_EDGE_INTERFACE_AUDIO_CONTROL
    )
  );
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;

  const auto result = device.handle_submit(request, report);

  EXPECT_FALSE(result.has_output_report);
}

TEST(DualSenseUsbIpTests, DeviceReturnsErrorForUnsupportedSubmitEndpoint) {
  platf::dualsense_usbip::device_t device;
  const auto result = device.handle_submit(submit_request(
    23,
    platf::usbip::direction::in,
    9,
    64
  ));

  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 23);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 20), static_cast<std::uint32_t>(-1));
  EXPECT_FALSE(result.has_output_report);
}

TEST(DualSenseUsbIpTests, DeviceRejectsUnsupportedOutEndpointAfterPayloadIsRead) {
  platf::dualsense_usbip::device_t device;
  const auto result = device.handle_submit(
    submit_request(24, platf::usbip::direction::out, 1, 392),
    std::vector<std::uint8_t>(392)
  );

  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 24);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 20), static_cast<std::uint32_t>(-1));
  EXPECT_FALSE(result.has_output_report);
}

TEST(DualSenseUsbIpTests, DeviceRejectsSubmitsForDifferentDevid) {
  platf::dualsense_usbip::device_t device;
  auto request = submit_request(
    25,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  );
  request.header.devid = platf::dualsense_usbip::DEVID + 1;

  const auto result = device.handle_submit(request);

  EXPECT_FALSE(result.has_output_report);
  EXPECT_TRUE(submit_payload(result.reply).empty());
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 8), platf::dualsense_usbip::DEVID + 1);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 20), static_cast<std::uint32_t>(-1));
}

TEST(DualSenseUsbIpTests, SessionQueuesInterruptInSubmitsUntilTaken) {
  platf::dualsense_usbip::session_t session;
  session.update_input(platf::gamepad_state_t {
    .buttonFlags = platf::PADDLE1,
    .lt = 0,
    .rt = 0,
    .lsX = 0,
    .lsY = 0,
    .rsX = 0,
    .rsY = 0,
  });

  const auto result = session.handle_submit(submit_request(
    30,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));

  EXPECT_TRUE(result.reply.empty());
  EXPECT_TRUE(session.has_pending_interrupt_in());
  EXPECT_EQ(session.pending_interrupt_in_count(), 1);

  const auto reply = session.take_next_interrupt_in_reply();
  const auto payload = submit_payload(reply);

  EXPECT_FALSE(reply.empty());
  EXPECT_FALSE(session.has_pending_interrupt_in());
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 30);
  ASSERT_EQ(payload.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(payload[10], platf::dualsense::BUTTONS2_EDGE_RIGHT_PADDLE);
}

TEST(DualSenseUsbIpTests, SessionBoundsPendingInterruptInSubmits) {
  platf::dualsense_usbip::session_t session;

  for (std::size_t i = 0; i < platf::dualsense_usbip::MAX_PENDING_INTERRUPT_IN; ++i) {
    const auto result = session.handle_submit(submit_request(
      static_cast<std::uint32_t>(40 + i),
      platf::usbip::direction::in,
      platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
      platf::dualsense::INPUT_REPORT_USB_SIZE
    ));
    EXPECT_TRUE(result.reply.empty());
  }

  ASSERT_EQ(session.pending_interrupt_in_count(), platf::dualsense_usbip::MAX_PENDING_INTERRUPT_IN);

  const auto overflow = session.handle_submit(submit_request(
    100,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));

  EXPECT_FALSE(overflow.reply.empty());
  EXPECT_EQ(session.pending_interrupt_in_count(), platf::dualsense_usbip::MAX_PENDING_INTERRUPT_IN);
  EXPECT_EQ(platf::usbip::read_be32(overflow.reply, 4), 100);
  EXPECT_EQ(submit_payload(overflow.reply).size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
}

TEST(DualSenseUsbIpTests, SessionUnlinksQueuedInterruptInSubmit) {
  platf::dualsense_usbip::session_t session;
  session.handle_submit(submit_request(
    31,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));

  ASSERT_EQ(session.pending_interrupt_in_count(), 1);

  const auto result = session.handle_unlink(unlink_request(32, 31));

  EXPECT_TRUE(result.removed);
  EXPECT_EQ(session.pending_interrupt_in_count(), 0);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_unlink));
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 32);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 20), 0);
  EXPECT_TRUE(session.take_next_interrupt_in_reply().empty());
}

TEST(DualSenseUsbIpTests, SessionReportsFailedUnlinkForUnknownSequence) {
  platf::dualsense_usbip::session_t session;

  const auto result = session.handle_unlink(unlink_request(33, 99));

  EXPECT_FALSE(result.removed);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_unlink));
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 4), 33);
  EXPECT_EQ(platf::usbip::read_be32(result.reply, 20), static_cast<std::uint32_t>(-1));
}

TEST(DualSenseUsbIpTests, SessionRejectsSubmitsAndUnlinksForDifferentDevid) {
  platf::dualsense_usbip::session_t session;
  auto submit = submit_request(
    36,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_INTERRUPT_IN,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  );
  submit.header.devid = platf::dualsense_usbip::DEVID + 1;

  const auto submit_result = session.handle_submit(submit);

  EXPECT_FALSE(submit_result.reply.empty());
  EXPECT_FALSE(submit_result.has_output_report);
  EXPECT_FALSE(session.has_pending_interrupt_in());
  EXPECT_EQ(platf::usbip::read_be32(submit_result.reply, 8), platf::dualsense_usbip::DEVID + 1);
  EXPECT_EQ(platf::usbip::read_be32(submit_result.reply, 20), static_cast<std::uint32_t>(-1));

  auto unlink = unlink_request(37, 36);
  unlink.header.devid = platf::dualsense_usbip::DEVID + 1;

  const auto unlink_result = session.handle_unlink(unlink);

  EXPECT_FALSE(unlink_result.removed);
  EXPECT_EQ(platf::usbip::read_be32(unlink_result.reply, 8), platf::dualsense_usbip::DEVID + 1);
  EXPECT_EQ(platf::usbip::read_be32(unlink_result.reply, 20), static_cast<std::uint32_t>(-1));
}

TEST(DualSenseUsbIpTests, SessionPassesThroughImmediateControlAndOutputSubmits) {
  platf::dualsense_usbip::session_t session;

  const auto control = session.handle_submit(submit_request(
    34,
    platf::usbip::direction::in,
    platf::dualsense_usbip::ENDPOINT_CONTROL,
    platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE,
    get_descriptor_setup(platf::usb::DESCRIPTOR_HID_REPORT, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE)
  ));
  EXPECT_FALSE(control.reply.empty());
  EXPECT_FALSE(control.has_output_report);
  EXPECT_EQ(platf::usbip::read_be32(control.reply, 4), 34);

  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[4] = 0x80;

  const auto output = session.handle_submit(
    submit_request(
      35,
      platf::usbip::direction::out,
      platf::dualsense_usbip::ENDPOINT_INTERRUPT_OUT,
      platf::dualsense::OUTPUT_REPORT_USB_SIZE
    ),
    report
  );

  EXPECT_FALSE(output.reply.empty());
  EXPECT_TRUE(output.has_output_report);
  EXPECT_TRUE(output.output_report.has_rumble);
  EXPECT_EQ(output.output_report.motor_left, 0x80);
  EXPECT_EQ(platf::usbip::read_be32(output.reply, 4), 35);
}
