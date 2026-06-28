/**
 * @file tests/unit/platform/test_usbip.cpp
 * @brief Test src/platform/usbip.h.
 */
#include "../../tests_common.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <src/platform/dualsense.h>
#include <src/platform/usb.h>
#include <src/platform/usbip.h>

namespace {
  platf::usbip::device_identity_t edge_device() {
    return platf::usbip::device_identity_t {
      .path = "/sys/devices/platform/apollo-dualsense-edge",
      .busid = "1-1",
      .busnum = 1,
      .devnum = 2,
      .speed = platf::usbip::device_speed::high,
      .vendor_id = platf::dualsense::VENDOR_ID_SONY,
      .product_id = platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE,
      .bcd_device = 0x0100,
      .device_class = 0,
      .device_subclass = 0,
      .device_protocol = 0,
      .configuration_value = 1,
      .configuration_count = 1,
      .interface_count = 4,
    };
  }

  std::string fixed_string_at(const std::vector<std::uint8_t> &data, std::size_t offset, std::size_t width) {
    std::size_t end = offset;
    while (end < offset + width && data[end] != 0) {
      ++end;
    }

    return std::string(reinterpret_cast<const char *>(data.data() + offset), end - offset);
  }
}  // namespace

TEST(UsbIpTests, BuildsDualSenseEdgeImportReply) {
  const auto reply = platf::usbip::make_import_reply(edge_device());

  EXPECT_EQ(reply.size(), platf::usbip::OP_COMMON_SIZE + platf::usbip::USB_DEVICE_RECORD_SIZE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 0), platf::usbip::VERSION);
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 0);

  EXPECT_EQ(fixed_string_at(reply, 8, platf::usbip::USB_DEVICE_PATH_SIZE), "/sys/devices/platform/apollo-dualsense-edge");
  EXPECT_EQ(fixed_string_at(reply, 264, platf::usbip::USB_DEVICE_BUSID_SIZE), "1-1");
  EXPECT_EQ(platf::usbip::read_be32(reply, 296), 1);
  EXPECT_EQ(platf::usbip::read_be32(reply, 300), 2);
  EXPECT_EQ(platf::usbip::read_be32(reply, 304), static_cast<std::uint32_t>(platf::usbip::device_speed::high));
  EXPECT_EQ(platf::usbip::read_be16(reply, 308), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(platf::usbip::read_be16(reply, 310), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 312), 0x0100);
  EXPECT_EQ(reply[314], 0);
  EXPECT_EQ(reply[317], 1);
  EXPECT_EQ(reply[318], 1);
  EXPECT_EQ(reply[319], 4);
}

TEST(UsbIpTests, ParsesUsbIpOpCommonHeader) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_be16(request, platf::usbip::VERSION);
  platf::usbip::append_be16(request, static_cast<std::uint16_t>(platf::usbip::op_code::req_devlist));
  platf::usbip::append_be32(request, 0);

  const auto common = platf::usbip::parse_op_common(request);

  EXPECT_EQ(common.version, platf::usbip::VERSION);
  EXPECT_EQ(common.code, platf::usbip::op_code::req_devlist);
  EXPECT_EQ(common.status, 0);
}

TEST(UsbIpTests, ReadHelpersHandleShortBuffers) {
  const std::array<std::uint8_t, 3> bytes {0x01, 0x02, 0x03};

  EXPECT_TRUE(platf::usbip::has_bytes(bytes, 1, 2));
  EXPECT_FALSE(platf::usbip::has_bytes(bytes, 2, 2));
  EXPECT_EQ(platf::usbip::read_be16(bytes, 2), 0);
  EXPECT_EQ(platf::usbip::read_be32(bytes, 0), 0);
  EXPECT_EQ(platf::usbip::read_fixed_string(bytes, 4, 2), "");
}

TEST(UsbIpTests, ParsesUsbIpImportRequestBusId) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_be16(request, platf::usbip::VERSION);
  platf::usbip::append_be16(request, static_cast<std::uint16_t>(platf::usbip::op_code::req_import));
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_fixed_string(request, "1-1", platf::usbip::USB_DEVICE_BUSID_SIZE);

  const auto import = platf::usbip::parse_import_request(request);

  EXPECT_EQ(import.common.version, platf::usbip::VERSION);
  EXPECT_EQ(import.common.code, platf::usbip::op_code::req_import);
  EXPECT_EQ(import.busid, "1-1");
}

TEST(UsbIpTests, BuildsImportErrorReply) {
  const auto reply = platf::usbip::make_import_error_reply(1);

  EXPECT_EQ(reply.size(), platf::usbip::OP_COMMON_SIZE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 0), platf::usbip::VERSION);
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_import));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 1);
}

TEST(UsbIpTests, BuildsDeviceListReplyWithCompositeEdgeInterfaces) {
  const std::array interfaces {
    platf::usbip::interface_identity_t {
      .interface_class = 0x01,
      .interface_subclass = 0x01,
      .interface_protocol = 0x00,
    },
    platf::usbip::interface_identity_t {
      .interface_class = 0x01,
      .interface_subclass = 0x02,
      .interface_protocol = 0x00,
    },
    platf::usbip::interface_identity_t {
      .interface_class = 0x01,
      .interface_subclass = 0x02,
      .interface_protocol = 0x00,
    },
    platf::usbip::interface_identity_t {
      .interface_class = 0x03,
      .interface_subclass = 0x00,
      .interface_protocol = 0x00,
    },
  };

  const auto reply = platf::usbip::make_devlist_reply(edge_device(), interfaces);
  const auto interface_offset = platf::usbip::OP_COMMON_SIZE + sizeof(std::uint32_t) + platf::usbip::USB_DEVICE_RECORD_SIZE;

  EXPECT_EQ(reply.size(), interface_offset + interfaces.size() * platf::usbip::USB_INTERFACE_RECORD_SIZE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 0), platf::usbip::VERSION);
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_devlist));
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 1);
  EXPECT_EQ(platf::usbip::read_be16(reply, 314), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(reply[323], 4);
  EXPECT_EQ(reply[interface_offset], 0x01);
  EXPECT_EQ(reply[interface_offset + 1], 0x01);
  EXPECT_EQ(reply[interface_offset + platf::usbip::USB_INTERFACE_RECORD_SIZE], 0x01);
  EXPECT_EQ(reply[interface_offset + 2 * platf::usbip::USB_INTERFACE_RECORD_SIZE], 0x01);
  EXPECT_EQ(reply[interface_offset + 3 * platf::usbip::USB_INTERFACE_RECORD_SIZE], 0x03);
  EXPECT_EQ(reply[interface_offset + 3 * platf::usbip::USB_INTERFACE_RECORD_SIZE + 1], 0x00);
  EXPECT_EQ(reply[interface_offset + 3 * platf::usbip::USB_INTERFACE_RECORD_SIZE + 2], 0x00);
  EXPECT_EQ(reply[interface_offset + 3 * platf::usbip::USB_INTERFACE_RECORD_SIZE + 3], 0x00);
}

TEST(UsbIpTests, BuildsSubmitReplyWithPayload) {
  const std::array<std::uint8_t, 4> payload {
    platf::dualsense::INPUT_REPORT_USB,
    0xAA,
    0xBB,
    0xCC,
  };

  const auto reply = platf::usbip::make_ret_submit(
    0x10203040,
    0x50607080,
    platf::usbip::direction::in,
    4,
    0,
    payload
  );

  EXPECT_EQ(reply.size(), platf::usbip::SUBMIT_HEADER_SIZE + payload.size());
  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_submit));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 0x10203040);
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 0x50607080);
  EXPECT_EQ(platf::usbip::read_be32(reply, 12), static_cast<std::uint32_t>(platf::usbip::direction::in));
  EXPECT_EQ(platf::usbip::read_be32(reply, 16), 4);
  EXPECT_EQ(platf::usbip::read_be32(reply, 20), 0);
  EXPECT_EQ(platf::usbip::read_be32(reply, 24), static_cast<std::uint32_t>(payload.size()));
  EXPECT_EQ(platf::usbip::read_be32(reply, 32), static_cast<std::uint32_t>(platf::usbip::NUMBER_OF_PACKETS_NON_ISOCHRONOUS));
  EXPECT_EQ(platf::usbip::read_be32(reply, 36), 0);
  EXPECT_EQ(std::vector<std::uint8_t>(reply.begin() + platf::usbip::SUBMIT_HEADER_SIZE, reply.end()),
            std::vector<std::uint8_t>(payload.begin(), payload.end()));
}

TEST(UsbIpTests, ParsesSubmitRequestWithControlSetupPacket) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_basic_header(
    request,
    platf::usbip::command::cmd_submit,
    0x10203040,
    0x50607080,
    platf::usbip::direction::in,
    0
  );
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_be32(request, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_be32(request, 0);
  platf::usbip::append_be32(request, 0);
  request.insert(request.end(), {
    0x80,
    platf::usb::REQUEST_GET_DESCRIPTOR,
    0x00,
    platf::usb::DESCRIPTOR_HID_REPORT,
    0x00,
    0x00,
    0x85,
    0x01,
  });

  ASSERT_EQ(request.size(), platf::usbip::SUBMIT_HEADER_SIZE);

  const auto parsed = platf::usbip::parse_submit_request(request);
  const auto setup = platf::usb::parse_setup_packet(parsed.setup);

  EXPECT_EQ(parsed.header.command_code, platf::usbip::command::cmd_submit);
  EXPECT_EQ(parsed.header.seqnum, 0x10203040);
  EXPECT_EQ(parsed.header.devid, 0x50607080);
  EXPECT_EQ(parsed.header.transfer_direction, platf::usbip::direction::in);
  EXPECT_EQ(parsed.header.endpoint, 0);
  EXPECT_EQ(parsed.transfer_buffer_length, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(setup.request, platf::usb::REQUEST_GET_DESCRIPTOR);
  EXPECT_EQ(platf::usb::descriptor_type(setup.value), platf::usb::DESCRIPTOR_HID_REPORT);
  EXPECT_EQ(setup.length, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
}

TEST(UsbIpTests, ParsesShortSubmitRequestWithoutReadingSetup) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_basic_header(
    request,
    platf::usbip::command::cmd_submit,
    0x10203040,
    0x50607080,
    platf::usbip::direction::in,
    0
  );

  ASSERT_EQ(request.size(), platf::usbip::BASIC_HEADER_SIZE);

  const auto parsed = platf::usbip::parse_submit_request(request);

  EXPECT_EQ(parsed.header.command_code, platf::usbip::command::cmd_submit);
  EXPECT_EQ(parsed.header.seqnum, 0x10203040);
  EXPECT_EQ(parsed.header.devid, 0x50607080);
  EXPECT_EQ(parsed.transfer_buffer_length, 0);
  EXPECT_EQ(parsed.setup, (std::array<std::uint8_t, 8> {}));
}

TEST(UsbIpTests, ParsesUnlinkRequest) {
  std::vector<std::uint8_t> request;
  platf::usbip::append_basic_header(
    request,
    platf::usbip::command::cmd_unlink,
    0x10203040,
    0x50607080,
    platf::usbip::direction::out,
    0
  );
  platf::usbip::append_be32(request, 0x01020304);
  for (std::size_t i = 0; i < 24; ++i) {
    platf::usbip::append_u8(request, 0);
  }

  ASSERT_EQ(request.size(), platf::usbip::SUBMIT_HEADER_SIZE);

  const auto parsed = platf::usbip::parse_unlink_request(request);

  EXPECT_EQ(parsed.header.command_code, platf::usbip::command::cmd_unlink);
  EXPECT_EQ(parsed.header.seqnum, 0x10203040);
  EXPECT_EQ(parsed.header.devid, 0x50607080);
  EXPECT_EQ(parsed.unlink_seqnum, 0x01020304);
}

TEST(UsbIpTests, BuildsUnlinkReplyWithSignedStatus) {
  const auto reply = platf::usbip::make_ret_unlink(3, 4, -2);

  EXPECT_EQ(reply.size(), platf::usbip::SUBMIT_HEADER_SIZE);
  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_unlink));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 3);
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 4);
  EXPECT_EQ(platf::usbip::read_be32(reply, 20), static_cast<std::uint32_t>(-2));
}
