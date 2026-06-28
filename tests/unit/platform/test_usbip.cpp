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
#include <src/platform/usbip.h>

namespace {
  platf::usbip::device_identity_t edge_device() {
    return platf::usbip::device_identity_t {
      .path = "/sys/devices/platform/apollo-dualsense-edge",
      .busid = "1-1",
      .busnum = 1,
      .devnum = 2,
      .speed = platf::usbip::device_speed::full,
      .vendor_id = platf::dualsense::VENDOR_ID_SONY,
      .product_id = platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE,
      .bcd_device = 0x0100,
      .device_class = 0,
      .device_subclass = 0,
      .device_protocol = 0,
      .configuration_value = 1,
      .configuration_count = 1,
      .interface_count = 1,
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
  EXPECT_EQ(platf::usbip::read_be32(reply, 304), static_cast<std::uint32_t>(platf::usbip::device_speed::full));
  EXPECT_EQ(platf::usbip::read_be16(reply, 308), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(platf::usbip::read_be16(reply, 310), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 312), 0x0100);
  EXPECT_EQ(reply[314], 0);
  EXPECT_EQ(reply[317], 1);
  EXPECT_EQ(reply[318], 1);
  EXPECT_EQ(reply[319], 1);
}

TEST(UsbIpTests, BuildsDeviceListReplyWithHidInterface) {
  const std::array interfaces {
    platf::usbip::interface_identity_t {
      .interface_class = 0x03,
      .interface_subclass = 0x00,
      .interface_protocol = 0x00,
    },
  };

  const auto reply = platf::usbip::make_devlist_reply(edge_device(), interfaces);
  const auto interface_offset = platf::usbip::OP_COMMON_SIZE + sizeof(std::uint32_t) + platf::usbip::USB_DEVICE_RECORD_SIZE;

  EXPECT_EQ(reply.size(), interface_offset + platf::usbip::USB_INTERFACE_RECORD_SIZE);
  EXPECT_EQ(platf::usbip::read_be16(reply, 0), platf::usbip::VERSION);
  EXPECT_EQ(platf::usbip::read_be16(reply, 2), static_cast<std::uint16_t>(platf::usbip::op_code::rep_devlist));
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 1);
  EXPECT_EQ(platf::usbip::read_be16(reply, 314), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
  EXPECT_EQ(reply[interface_offset], 0x03);
  EXPECT_EQ(reply[interface_offset + 1], 0x00);
  EXPECT_EQ(reply[interface_offset + 2], 0x00);
  EXPECT_EQ(reply[interface_offset + 3], 0x00);
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
  EXPECT_EQ(std::vector<std::uint8_t>(reply.begin() + platf::usbip::SUBMIT_HEADER_SIZE, reply.end()),
            std::vector<std::uint8_t>(payload.begin(), payload.end()));
}

TEST(UsbIpTests, BuildsUnlinkReplyWithSignedStatus) {
  const auto reply = platf::usbip::make_ret_unlink(3, 4, -2);

  EXPECT_EQ(reply.size(), platf::usbip::SUBMIT_HEADER_SIZE);
  EXPECT_EQ(platf::usbip::read_be32(reply, 0), static_cast<std::uint32_t>(platf::usbip::command::ret_unlink));
  EXPECT_EQ(platf::usbip::read_be32(reply, 4), 3);
  EXPECT_EQ(platf::usbip::read_be32(reply, 8), 4);
  EXPECT_EQ(platf::usbip::read_be32(reply, 20), static_cast<std::uint32_t>(-2));
}
