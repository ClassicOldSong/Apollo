/**
 * @file tests/unit/platform/test_dualsense.cpp
 * @brief Test src/platform/dualsense.h.
 */
#include "../../tests_common.h"

#include <cstdint>
#include <limits>

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
}

TEST(DualSenseReportTests, BuildsUsbInputReportWithReportId) {
  auto state = state_with_buttons(platf::PADDLE4);

  const auto report = platf::dualsense::make_usb_input_report(state, 9, true);

  EXPECT_EQ(sizeof(report), platf::dualsense::INPUT_REPORT_USB_SIZE);
  EXPECT_EQ(report.report_id, platf::dualsense::INPUT_REPORT_USB);
  EXPECT_EQ(report.body.sequence_number, 9);
  EXPECT_EQ(report.body.buttons[2], platf::dualsense::BUTTONS2_EDGE_FN_LEFT);
}

TEST(DualSenseReportTests, DefinesDualSenseEdgeUsbIdentity) {
  const auto &device = platf::dualsense::DUALSENSE_EDGE_DEVICE_DESCRIPTOR;

  EXPECT_EQ(device.size(), 18);
  EXPECT_EQ(device[0], 0x12);
  EXPECT_EQ(device[1], 0x01);
  EXPECT_EQ(device[8] | (device[9] << 8), platf::dualsense::VENDOR_ID_SONY);
  EXPECT_EQ(device[10] | (device[11] << 8), platf::dualsense::PRODUCT_ID_DUALSENSE_EDGE);
}

TEST(DualSenseReportTests, DefinesMinimalDualSenseEdgeHidConfiguration) {
  const auto &config = platf::dualsense::DUALSENSE_EDGE_CONFIGURATION_DESCRIPTOR;
  const auto report_descriptor_size = config[25] | (config[26] << 8);

  EXPECT_EQ(config.size(), 41);
  EXPECT_EQ(config[0], 0x09);
  EXPECT_EQ(config[1], 0x02);
  EXPECT_EQ(config[2] | (config[3] << 8), config.size());
  EXPECT_EQ(config[18], 0x09);
  EXPECT_EQ(config[19], 0x21);
  EXPECT_EQ(config[24], 0x22);
  EXPECT_EQ(report_descriptor_size, platf::dualsense::DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR_SIZE);
  EXPECT_EQ(config[29], platf::dualsense::USB_ENDPOINT_IN);
  EXPECT_EQ(config[36], platf::dualsense::USB_ENDPOINT_OUT);
}
