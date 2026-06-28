/**
 * @file tests/unit/platform/test_dualsense.cpp
 * @brief Test src/platform/dualsense.h.
 */
#include "../../tests_common.h"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <src/platform/dualsense.h>

namespace {
  platf::gamepad_state_t state_with_buttons(std::uint32_t flags) {
    return platf::gamepad_state_t {
      .buttonFlags = flags,
      .lt = 0,
      .rt = 0,
      .lsX = 0,
      .lsY = 0,
      .rsX = 0,
      .rsY = 0,
    };
  }

  bool descriptor_has_report_id(std::uint8_t report_id) {
    const auto &descriptor = platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR;
    for (std::size_t i = 1; i < descriptor.size(); ++i) {
      if (descriptor[i - 1] == 0x85 && descriptor[i] == report_id) {
        return true;
      }
    }

    return false;
  }

  std::vector<std::pair<std::uint8_t, std::size_t>> feature_report_sizes_from_descriptor() {
    std::vector<std::pair<std::uint8_t, std::size_t>> sizes;
    const auto &descriptor = platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR;
    std::uint8_t report_id = 0;
    std::uint32_t report_size = 0;
    std::uint32_t report_count = 0;

    for (std::size_t i = 0; i < descriptor.size();) {
      const auto item = descriptor[i++];
      if (item == 0xFE) {
        if (i >= descriptor.size()) {
          break;
        }
        i += static_cast<std::size_t>(descriptor[i]) + 2;
        continue;
      }

      std::size_t data_size = item & 0x03;
      if (data_size == 3) {
        data_size = 4;
      }

      std::uint32_t value = 0;
      for (std::size_t byte = 0; byte < data_size && i + byte < descriptor.size(); ++byte) {
        value |= static_cast<std::uint32_t>(descriptor[i + byte]) << (byte * 8);
      }
      i += data_size;

      switch (item) {
        case 0x75:  // Report Size
          report_size = value;
          break;

        case 0x85:  // Report ID
          report_id = static_cast<std::uint8_t>(value);
          break;

        case 0x95:  // Report Count
          report_count = value;
          break;

        case 0xB1:  // Feature
          sizes.emplace_back(report_id, 1 + ((report_count * report_size + 7) / 8));
          break;

        default:
          break;
      }
    }

    return sizes;
  }

  platf::usb::setup_packet_t get_descriptor_setup(std::uint8_t descriptor_type, std::uint8_t descriptor_index, std::uint16_t length) {
    return platf::usb::setup_packet_t {
      .request_type = 0x80,
      .request = platf::usb::REQUEST_GET_DESCRIPTOR,
      .value = static_cast<std::uint16_t>((descriptor_type << 8) | descriptor_index),
      .index = 0,
      .length = length,
    };
  }

  platf::usb::setup_packet_t control_setup(std::uint8_t request_type, std::uint8_t request, std::uint16_t value, std::uint16_t length) {
    return platf::usb::setup_packet_t {
      .request_type = request_type,
      .request = request,
      .value = value,
      .index = 0,
      .length = length,
    };
  }

  platf::usb::setup_packet_t get_report_setup(std::uint8_t report_type, std::uint8_t report_id, std::uint16_t length) {
    return platf::usb::setup_packet_t {
      .request_type = platf::usb::REQUEST_DIRECTION_IN |
                      (platf::usb::REQUEST_TYPE_CLASS << 5) |
                      platf::usb::RECIPIENT_INTERFACE,
      .request = platf::usb::HID_REQUEST_GET_REPORT,
      .value = static_cast<std::uint16_t>((report_type << 8) | report_id),
      .index = 0,
      .length = length,
    };
  }
}  // namespace

TEST(DualSenseReportTests, PacksDpadAndFaceButtons) {
  const auto report = platf::dualsense::pack_buttons(
    platf::DPAD_UP | platf::DPAD_RIGHT | platf::A | platf::B | platf::X | platf::Y,
    0,
    0,
    false
  );

  EXPECT_EQ(report.buttons0, platf::dualsense::HAT_NORTHEAST |
                               platf::dualsense::BUTTONS0_SQUARE |
                               platf::dualsense::BUTTONS0_CROSS |
                               platf::dualsense::BUTTONS0_CIRCLE |
                               platf::dualsense::BUTTONS0_TRIANGLE);
  EXPECT_EQ(report.buttons1, 0);
  EXPECT_EQ(report.buttons2, 0);
}

TEST(DualSenseReportTests, PacksShouldersTriggersAndSystemButtons) {
  const auto report = platf::dualsense::pack_buttons(
    platf::LEFT_BUTTON | platf::RIGHT_BUTTON | platf::BACK | platf::START |
      platf::LEFT_STICK | platf::RIGHT_STICK | platf::HOME | platf::TOUCHPAD_BUTTON |
      platf::MISC_BUTTON,
    1,
    255,
    false
  );

  EXPECT_EQ(report.buttons0, platf::dualsense::HAT_NONE);
  EXPECT_EQ(report.buttons1, platf::dualsense::BUTTONS1_L1 |
                               platf::dualsense::BUTTONS1_R1 |
                               platf::dualsense::BUTTONS1_L2 |
                               platf::dualsense::BUTTONS1_R2 |
                               platf::dualsense::BUTTONS1_CREATE |
                               platf::dualsense::BUTTONS1_OPTIONS |
                               platf::dualsense::BUTTONS1_L3 |
                               platf::dualsense::BUTTONS1_R3);
  EXPECT_EQ(report.buttons2, platf::dualsense::BUTTONS2_PS_HOME |
                               platf::dualsense::BUTTONS2_TOUCHPAD |
                               platf::dualsense::BUTTONS2_MIC_MUTE);
}

TEST(DualSenseReportTests, PacksDualSenseEdgeButtonsOnlyWhenRequested) {
  const auto flags = platf::PADDLE1 | platf::PADDLE2 | platf::PADDLE3 | platf::PADDLE4;

  const auto standard_report = platf::dualsense::pack_buttons(flags, 0, 0, false);
  EXPECT_EQ(standard_report.buttons2, 0);

  const auto edge_report = platf::dualsense::pack_buttons(flags, 0, 0, true);
  EXPECT_EQ(edge_report.buttons2, platf::dualsense::BUTTONS2_EDGE_RIGHT_PADDLE |
                                    platf::dualsense::BUTTONS2_EDGE_LEFT_PADDLE |
                                    platf::dualsense::BUTTONS2_EDGE_FN_RIGHT |
                                    platf::dualsense::BUTTONS2_EDGE_FN_LEFT);
}

TEST(DualSenseReportTests, PreservesSdlDualSenseEdgePaddleOrder) {
  EXPECT_EQ(
    platf::dualsense::pack_buttons(platf::PADDLE1, 0, 0, true).buttons2,
    platf::dualsense::BUTTONS2_EDGE_RIGHT_PADDLE
  );
  EXPECT_EQ(
    platf::dualsense::pack_buttons(platf::PADDLE2, 0, 0, true).buttons2,
    platf::dualsense::BUTTONS2_EDGE_LEFT_PADDLE
  );
  EXPECT_EQ(
    platf::dualsense::pack_buttons(platf::PADDLE3, 0, 0, true).buttons2,
    platf::dualsense::BUTTONS2_EDGE_FN_RIGHT
  );
  EXPECT_EQ(
    platf::dualsense::pack_buttons(platf::PADDLE4, 0, 0, true).buttons2,
    platf::dualsense::BUTTONS2_EDGE_FN_LEFT
  );
}

TEST(DualSenseReportTests, BuildsUsbInputReportPayload) {
  auto state = state_with_buttons(platf::A | platf::PADDLE1);
  state.lt = 64;
  state.rt = 128;
  state.lsX = std::numeric_limits<std::int16_t>::min();
  state.rsX = std::numeric_limits<std::int16_t>::max();

  const auto report = platf::dualsense::make_input_report(state, 7, true);

  EXPECT_EQ(sizeof(report), platf::dualsense::INPUT_REPORT_USB_SIZE - 1);
  EXPECT_EQ(report.x, 0);
  EXPECT_EQ(report.rx, 255);
  EXPECT_EQ(report.z, 64);
  EXPECT_EQ(report.rz, 128);
  EXPECT_EQ(report.sequence_number, 7);
  EXPECT_EQ(report.buttons[0], platf::dualsense::HAT_NONE | platf::dualsense::BUTTONS0_CROSS);
  EXPECT_EQ(report.buttons[1], platf::dualsense::BUTTONS1_L2 | platf::dualsense::BUTTONS1_R2);
  EXPECT_EQ(report.buttons[2], platf::dualsense::BUTTONS2_EDGE_RIGHT_PADDLE);
  EXPECT_EQ(report.buttons[3], 0);
  EXPECT_EQ(report.touch_points[0], 0x80);
  EXPECT_EQ(report.touch_points[4], 0x80);
}

TEST(DualSenseReportTests, BuildsUsbInputReportWithReportId) {
  auto state = state_with_buttons(platf::PADDLE4);

  const auto report = platf::dualsense::make_usb_input_report(state, 9, true);

  EXPECT_EQ(sizeof(report), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(report.report_id, platf::dualsense::INPUT_REPORT_USB);
  EXPECT_EQ(report.body.sequence_number, 9);
  EXPECT_EQ(report.body.buttons[2], platf::dualsense::BUTTONS2_EDGE_FN_LEFT);
}

TEST(DualSenseReportTests, ParsesUsbOutputRumbleAndLightbarReport) {
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[1] = platf::dualsense::OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
  report[2] = platf::dualsense::OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
  report[3] = 0x22;  // right motor
  report[4] = 0x44;  // left motor
  report[45] = 0x10;
  report[46] = 0x20;
  report[47] = 0x30;

  const auto state = platf::dualsense::parse_output_report(report);

  EXPECT_TRUE(state.valid);
  EXPECT_TRUE(state.has_rumble);
  EXPECT_EQ(state.motor_left, 0x44);
  EXPECT_EQ(state.motor_right, 0x22);
  EXPECT_TRUE(state.has_lightbar);
  EXPECT_EQ(state.lightbar_red, 0x10);
  EXPECT_EQ(state.lightbar_green, 0x20);
  EXPECT_EQ(state.lightbar_blue, 0x30);
}

TEST(DualSenseReportTests, ParsesUsbOutputRumbleV2Flag) {
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_MIN_SIZE> report {};
  report[0] = platf::dualsense::OUTPUT_REPORT_USB;
  report[39] = platf::dualsense::OUTPUT_VALID_FLAG2_COMPATIBLE_VIBRATION2;
  report[3] = 0x11;
  report[4] = 0x33;

  const auto state = platf::dualsense::parse_output_report(report);

  EXPECT_TRUE(state.valid);
  EXPECT_TRUE(state.has_rumble);
  EXPECT_EQ(state.motor_left, 0x33);
  EXPECT_EQ(state.motor_right, 0x11);
  EXPECT_FALSE(state.has_lightbar);
}

TEST(DualSenseReportTests, RejectsInvalidUsbOutputReport) {
  std::array<std::uint8_t, platf::dualsense::OUTPUT_REPORT_USB_MIN_SIZE> report {};
  report[0] = platf::dualsense::INPUT_REPORT_USB;

  const auto state = platf::dualsense::parse_output_report(report);

  EXPECT_FALSE(state.valid);
}

TEST(DualSenseReportTests, DefinesDualSenseEdgeUsbIdentity) {
  const auto &device = platf::dualsense::DUALSENSE_EDGE_DEVICE_DESCRIPTOR;

  EXPECT_EQ(device.size(), 18);
  EXPECT_EQ(device[0], 0x12);
  EXPECT_EQ(device[1], 0x01);
  EXPECT_EQ(device[8] | (device[9] << 8), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(device[10] | (device[11] << 8), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseReportTests, DefinesCapturedDualSenseEdgeCompositeConfiguration) {
  const auto &config = platf::dualsense::DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR;
  const auto hid_offset = platf::dualsense::DUALSENSE_EDGE_HID_DESCRIPTOR_OFFSET;
  const auto report_descriptor_size = config[hid_offset + 7] | (config[hid_offset + 8] << 8);

  EXPECT_EQ(config.size(), platf::dualsense::DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR_SIZE);
  EXPECT_EQ(config[0], 0x09);
  EXPECT_EQ(config[1], 0x02);
  EXPECT_EQ(config[2] | (config[3] << 8), config.size());
  EXPECT_EQ(config[4], 4);
  EXPECT_EQ(config[195], 0x09);
  EXPECT_EQ(config[196], 0x04);
  EXPECT_EQ(config[197], platf::dualsense::DUALSENSE_EDGE_INTERFACE_HID);
  EXPECT_EQ(config[200], 0x03);
  EXPECT_EQ(config[hid_offset], 0x09);
  EXPECT_EQ(config[hid_offset + 1], 0x21);
  EXPECT_EQ(config[hid_offset + 6], 0x22);
  EXPECT_EQ(report_descriptor_size, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(config[215], platf::dualsense::USB_ENDPOINT_IN);
  EXPECT_EQ(config[222], platf::dualsense::USB_ENDPOINT_OUT);
}

TEST(DualSenseReportTests, DefinesDualSenseEdgeHidReportDescriptor) {
  const auto &descriptor = platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR;

  EXPECT_EQ(descriptor.size(), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(descriptor.front(), 0x05);
  EXPECT_EQ(descriptor.back(), 0xC0);

  EXPECT_TRUE(descriptor_has_report_id(0x01));
  EXPECT_TRUE(descriptor_has_report_id(0x02));
  EXPECT_TRUE(descriptor_has_report_id(0x60));
  EXPECT_TRUE(descriptor_has_report_id(0x70));
  EXPECT_TRUE(descriptor_has_report_id(0x7B));
}

TEST(DualSenseReportTests, ProvidesDualSenseEdgeDescriptorsForUsbEnumeration) {
  const auto device = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_DEVICE, 0, 255)
  );
  EXPECT_EQ(device.size(), platf::dualsense::DUALSENSE_EDGE_DEVICE_DESCRIPTOR.size());
  EXPECT_EQ(device[8] | (device[9] << 8), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(device[10] | (device[11] << 8), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);

  const auto hid = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_HID, 0, 9)
  );
  ASSERT_EQ(hid.size(), 9);
  EXPECT_EQ(hid[0], 0x09);
  EXPECT_EQ(hid[1], platf::usb::DESCRIPTOR_HID);
  EXPECT_EQ(hid[7] | (hid[8] << 8), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);

  const auto report = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_HID_REPORT, 0, 1024)
  );
  EXPECT_EQ(report.size(), platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(report.front(), 0x05);
  EXPECT_EQ(report.back(), 0xC0);
}

TEST(DualSenseReportTests, ProvidesDualSenseEdgeStringDescriptorsForUsbEnumeration) {
  const auto language = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_STRING, 0, 255)
  );
  EXPECT_EQ(language, platf::usb::make_language_string_descriptor());

  const auto product = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_STRING, 2, 255)
  );
  ASSERT_GE(product.size(), 4);
  EXPECT_EQ(product[1], platf::usb::DESCRIPTOR_STRING);
  EXPECT_EQ(product[2], 'D');
  EXPECT_EQ(product[3], 0);
  EXPECT_EQ(product[4], 'u');
  EXPECT_EQ(product[5], 0);
}

TEST(DualSenseReportTests, LimitsDualSenseEdgeDescriptorResponsesToRequestedLength) {
  const auto response = platf::dualsense::dualsense_edge_control_response(
    get_descriptor_setup(platf::usb::DESCRIPTOR_DEVICE, 0, 4)
  );

  EXPECT_EQ(response.size(), 4);
  EXPECT_EQ(response[0], 0x12);
  EXPECT_EQ(response[1], platf::usb::DESCRIPTOR_DEVICE);
}

TEST(DualSenseReportTests, ProvidesDualSenseEdgeFeatureReportsForUsbEnumeration) {
  const auto calibration = platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_FEATURE,
    platf::dualsense::FEATURE_REPORT_CALIBRATION,
    platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE
  ));

  ASSERT_EQ(calibration.size(), platf::dualsense::FEATURE_REPORT_CALIBRATION_SIZE);
  EXPECT_EQ(calibration[0], platf::dualsense::FEATURE_REPORT_CALIBRATION);
  EXPECT_EQ(calibration[7], 0x00);
  EXPECT_EQ(calibration[8], 0x04);
  EXPECT_EQ(calibration[9], 0x00);
  EXPECT_EQ(calibration[10], 0xFC);
  EXPECT_EQ(calibration[23], 0x00);
  EXPECT_EQ(calibration[24], 0x20);
  EXPECT_EQ(calibration[25], 0x00);
  EXPECT_EQ(calibration[26], 0xE0);

  const auto pairing = platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_FEATURE,
    platf::dualsense::FEATURE_REPORT_PAIRING_INFO,
    platf::dualsense::FEATURE_REPORT_PAIRING_INFO_SIZE
  ));
  ASSERT_EQ(pairing.size(), platf::dualsense::FEATURE_REPORT_PAIRING_INFO_SIZE);
  EXPECT_EQ(pairing[0], platf::dualsense::FEATURE_REPORT_PAIRING_INFO);
  EXPECT_EQ(pairing[1], 0x02);
  EXPECT_EQ(pairing[5], 0x0D);
  EXPECT_EQ(pairing[6], 0xF2);

  const auto firmware = platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_FEATURE,
    platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO,
    platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO_SIZE
  ));
  ASSERT_EQ(firmware.size(), platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO_SIZE);
  EXPECT_EQ(firmware[0], platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO);
  EXPECT_EQ(firmware[24], 1);
  EXPECT_EQ(firmware[28], 1);
  EXPECT_EQ(firmware[44], 1);
}

TEST(DualSenseReportTests, ProvidesNeutralDualSenseEdgeInputReportForControlGetReport) {
  const auto report = platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_INPUT,
    platf::dualsense::INPUT_REPORT_USB,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  ));

  ASSERT_EQ(report.size(), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(report[0], platf::dualsense::INPUT_REPORT_USB);
  EXPECT_EQ(report[7], 0);
  EXPECT_EQ(report[8], platf::dualsense::HAT_NONE);
  EXPECT_EQ(report[10], 0);
  EXPECT_EQ(report[33], 0x80);
  EXPECT_EQ(report[37], 0x80);
}

TEST(DualSenseReportTests, DualSenseEdgeFeatureReportSizesMatchHidDescriptor) {
  const auto sizes = feature_report_sizes_from_descriptor();

  ASSERT_FALSE(sizes.empty());
  for (const auto &[report_id, expected_size] : sizes) {
    EXPECT_EQ(platf::dualsense::dualsense_edge_feature_report_size(report_id), expected_size)
      << "feature report 0x" << std::hex << static_cast<int>(report_id);

    const auto report = platf::dualsense::dualsense_edge_feature_report(report_id);
    ASSERT_EQ(report.size(), expected_size)
      << "feature report 0x" << std::hex << static_cast<int>(report_id);
    EXPECT_EQ(report[0], report_id);
  }
}

TEST(DualSenseReportTests, LimitsAndRejectsDualSenseEdgeFeatureReports) {
  const auto limited = platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_FEATURE,
    platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO,
    4
  ));
  ASSERT_EQ(limited.size(), 4);
  EXPECT_EQ(limited[0], platf::dualsense::FEATURE_REPORT_FIRMWARE_INFO);

  EXPECT_EQ(platf::dualsense::dualsense_edge_feature_report_size(0x08), 48);
  EXPECT_EQ(platf::dualsense::dualsense_edge_feature_report_size(0x7B), 64);
  EXPECT_EQ(platf::dualsense::dualsense_edge_feature_report_size(0xF2), 53);
  EXPECT_TRUE(platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_FEATURE,
    0x99,
    64
  )).empty());
  EXPECT_TRUE(platf::dualsense::dualsense_edge_control_response(get_report_setup(
    platf::usb::HID_REPORT_TYPE_INPUT,
    0x31,
    platf::dualsense::INPUT_REPORT_USB_SIZE
  )).empty());
}

TEST(DualSenseReportTests, HandlesDualSenseEdgeCommonControlRequests) {
  const auto standard_device_in = platf::usb::REQUEST_DIRECTION_IN |
                                  (platf::usb::REQUEST_TYPE_STANDARD << 5) |
                                  platf::usb::RECIPIENT_DEVICE;
  const auto standard_device_out = (platf::usb::REQUEST_TYPE_STANDARD << 5) |
                                   platf::usb::RECIPIENT_DEVICE;
  const auto standard_interface_in = platf::usb::REQUEST_DIRECTION_IN |
                                     (platf::usb::REQUEST_TYPE_STANDARD << 5) |
                                     platf::usb::RECIPIENT_INTERFACE;
  const auto class_interface_in = platf::usb::REQUEST_DIRECTION_IN |
                                  (platf::usb::REQUEST_TYPE_CLASS << 5) |
                                  platf::usb::RECIPIENT_INTERFACE;
  const auto class_interface_out = (platf::usb::REQUEST_TYPE_CLASS << 5) |
                                   platf::usb::RECIPIENT_INTERFACE;

  EXPECT_EQ(
    platf::dualsense::dualsense_edge_control_response(control_setup(standard_device_in, platf::usb::REQUEST_GET_STATUS, 0, 2)),
    (std::vector<std::uint8_t> {1, 0})
  );
  EXPECT_EQ(
    platf::dualsense::dualsense_edge_control_response(control_setup(standard_device_in, platf::usb::REQUEST_GET_CONFIGURATION, 0, 1)),
    (std::vector<std::uint8_t> {1})
  );
  EXPECT_TRUE(
    platf::dualsense::dualsense_edge_control_response(control_setup(standard_device_out, platf::usb::REQUEST_SET_CONFIGURATION, 1, 0)).empty()
  );
  EXPECT_EQ(
    platf::dualsense::dualsense_edge_control_response(control_setup(standard_interface_in, platf::usb::REQUEST_GET_INTERFACE, 0, 1)),
    (std::vector<std::uint8_t> {0})
  );
  EXPECT_EQ(
    platf::dualsense::dualsense_edge_control_response(control_setup(class_interface_in, platf::usb::HID_REQUEST_GET_IDLE, 0, 1)),
    (std::vector<std::uint8_t> {0})
  );
  EXPECT_EQ(
    platf::dualsense::dualsense_edge_control_response(control_setup(class_interface_in, platf::usb::HID_REQUEST_GET_PROTOCOL, 0, 1)),
    (std::vector<std::uint8_t> {1})
  );
  EXPECT_TRUE(
    platf::dualsense::dualsense_edge_control_response(control_setup(class_interface_out, platf::usb::HID_REQUEST_SET_IDLE, 0, 0)).empty()
  );
  EXPECT_TRUE(
    platf::dualsense::dualsense_edge_control_response(control_setup(class_interface_out, platf::usb::HID_REQUEST_SET_REPORT, 0x0202, 0)).empty()
  );
}
