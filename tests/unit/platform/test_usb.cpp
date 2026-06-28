/**
 * @file tests/unit/platform/test_usb.cpp
 * @brief Test src/platform/usb.h.
 */
#include "../../tests_common.h"

#include <array>
#include <cstdint>
#include <vector>

#include <src/platform/usb.h>

TEST(UsbTests, ParsesSetupPacket) {
  const std::array<std::uint8_t, 8> setup {
    0x80,
    platf::usb::REQUEST_GET_DESCRIPTOR,
    0x00,
    platf::usb::DESCRIPTOR_HID_REPORT,
    0x00,
    0x00,
    0x85,
    0x01,
  };

  const auto packet = platf::usb::parse_setup_packet(setup);

  EXPECT_EQ(packet.request_type, 0x80);
  EXPECT_EQ(packet.request, platf::usb::REQUEST_GET_DESCRIPTOR);
  EXPECT_EQ(packet.value, 0x2200);
  EXPECT_EQ(packet.index, 0);
  EXPECT_EQ(packet.length, 389);
  EXPECT_EQ(platf::usb::setup_type(packet.request_type), platf::usb::REQUEST_TYPE_STANDARD);
  EXPECT_EQ(platf::usb::descriptor_type(packet.value), platf::usb::DESCRIPTOR_HID_REPORT);
  EXPECT_EQ(platf::usb::descriptor_index(packet.value), 0);
}

TEST(UsbTests, BuildsLanguageStringDescriptor) {
  const auto descriptor = platf::usb::make_language_string_descriptor();

  ASSERT_EQ(descriptor.size(), 4);
  EXPECT_EQ(descriptor[0], 4);
  EXPECT_EQ(descriptor[1], platf::usb::DESCRIPTOR_STRING);
  EXPECT_EQ(descriptor[2], 0x09);
  EXPECT_EQ(descriptor[3], 0x04);
}

TEST(UsbTests, BuildsAsciiStringDescriptorAsUtf16Le) {
  const auto descriptor = platf::usb::make_ascii_string_descriptor("Edge");

  ASSERT_EQ(descriptor.size(), 10);
  EXPECT_EQ(descriptor[0], 10);
  EXPECT_EQ(descriptor[1], platf::usb::DESCRIPTOR_STRING);
  EXPECT_EQ(descriptor[2], 'E');
  EXPECT_EQ(descriptor[3], 0);
  EXPECT_EQ(descriptor[8], 'e');
  EXPECT_EQ(descriptor[9], 0);
}

TEST(UsbTests, TruncatesLimitedResponses) {
  const std::array<std::uint8_t, 5> payload {1, 2, 3, 4, 5};

  const auto response = platf::usb::limited_response(payload, 3);

  EXPECT_EQ(response, (std::vector<std::uint8_t> {1, 2, 3}));
}
