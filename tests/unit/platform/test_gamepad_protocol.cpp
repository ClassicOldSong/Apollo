/**
 * @file tests/unit/platform/test_gamepad_protocol.cpp
 * @brief Test Moonlight/Sunshine gamepad protocol button compatibility.
 */
#include "../../tests_common.h"

#include <cstdint>

extern "C" {
#include <moonlight-common-c/src/Input.h>
#include <moonlight-common-c/src/Limelight.h>
}

TEST(GamepadProtocolTests, PaddleFlagsMatchMoonlightSunshineProtocol) {
  EXPECT_EQ(static_cast<std::uint32_t>(PADDLE1_FLAG), platf::PADDLE1);
  EXPECT_EQ(static_cast<std::uint32_t>(PADDLE2_FLAG), platf::PADDLE2);
  EXPECT_EQ(static_cast<std::uint32_t>(PADDLE3_FLAG), platf::PADDLE3);
  EXPECT_EQ(static_cast<std::uint32_t>(PADDLE4_FLAG), platf::PADDLE4);
  EXPECT_EQ(
    static_cast<std::uint32_t>(PADDLE1_FLAG | PADDLE2_FLAG | PADDLE3_FLAG | PADDLE4_FLAG),
    platf::EDGE_PADDLE_BUTTONS
  );
}

TEST(GamepadProtocolTests, PaddleFlagsTravelInMultiControllerButtonFlags2) {
  constexpr std::uint32_t paddles =
    PADDLE1_FLAG | PADDLE2_FLAG | PADDLE3_FLAG | PADDLE4_FLAG;

  EXPECT_EQ(static_cast<std::uint16_t>(paddles), 0);
  EXPECT_EQ(static_cast<std::uint16_t>(paddles >> 16), 0x000F);

  NV_MULTI_CONTROLLER_PACKET packet {};
  packet.buttonFlags = 0;
  packet.buttonFlags2 = static_cast<short>(paddles >> 16);

  EXPECT_EQ(platf::make_gamepad_button_flags(packet.buttonFlags, packet.buttonFlags2), paddles);
}

TEST(GamepadProtocolTests, IndividualPaddleFlagsUseStableButtonFlags2Bits) {
  struct Case {
    std::uint32_t flag;
    std::uint16_t button_flags_2;
  };

  constexpr Case cases[] {
    { PADDLE1_FLAG, 0x0001 },
    { PADDLE2_FLAG, 0x0002 },
    { PADDLE3_FLAG, 0x0004 },
    { PADDLE4_FLAG, 0x0008 },
  };

  for (const auto &test_case : cases) {
    EXPECT_EQ(static_cast<std::uint16_t>(test_case.flag), 0);
    EXPECT_EQ(static_cast<std::uint16_t>(test_case.flag >> 16), test_case.button_flags_2);

    NV_MULTI_CONTROLLER_PACKET packet {};
    packet.buttonFlags = 0;
    packet.buttonFlags2 = static_cast<short>(test_case.button_flags_2);

    EXPECT_EQ(platf::make_gamepad_button_flags(packet.buttonFlags, packet.buttonFlags2), test_case.flag);
  }
}

TEST(GamepadProtocolTests, ButtonFlags2IsTreatedAsUnsignedTransportWord) {
  NV_MULTI_CONTROLLER_PACKET packet {};
  packet.buttonFlags = static_cast<short>(0x8001);
  packet.buttonFlags2 = static_cast<short>(0x8001);

  EXPECT_EQ(platf::make_gamepad_button_flags(packet.buttonFlags, packet.buttonFlags2), 0x80018001u);
}

TEST(GamepadProtocolTests, ControllerArrivalCanAdvertiseAllPaddleFlags) {
  constexpr std::uint32_t paddles =
    PADDLE1_FLAG | PADDLE2_FLAG | PADDLE3_FLAG | PADDLE4_FLAG;

  SS_CONTROLLER_ARRIVAL_PACKET packet {};
  packet.supportedButtonFlags = paddles | TOUCHPAD_FLAG | MISC_FLAG;

  EXPECT_EQ(packet.supportedButtonFlags & paddles, paddles);
}

TEST(GamepadProtocolTests, EdgeModeRequiresPlayStationArrivalWithCompletePaddleMask) {
  EXPECT_TRUE(platf::has_all_edge_paddle_buttons(platf::EDGE_PADDLE_BUTTONS));
  EXPECT_TRUE(platf::has_all_edge_paddle_buttons(
    platf::EDGE_PADDLE_BUTTONS | platf::TOUCHPAD_BUTTON | platf::MISC_BUTTON
  ));

  EXPECT_FALSE(platf::has_all_edge_paddle_buttons(0));

  constexpr std::uint32_t incomplete_paddle_masks[] {
    platf::PADDLE2 | platf::PADDLE3 | platf::PADDLE4,
    platf::PADDLE1 | platf::PADDLE3 | platf::PADDLE4,
    platf::PADDLE1 | platf::PADDLE2 | platf::PADDLE4,
    platf::PADDLE1 | platf::PADDLE2 | platf::PADDLE3,
  };
  for (const auto buttons : incomplete_paddle_masks) {
    EXPECT_FALSE(platf::has_all_edge_paddle_buttons(buttons));
    EXPECT_FALSE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_PS, buttons));
  }

  EXPECT_TRUE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_PS, platf::EDGE_PADDLE_BUTTONS));
  EXPECT_TRUE(platf::has_dualsense_edge_arrival_support(
    LI_CTYPE_PS,
    platf::EDGE_PADDLE_BUTTONS | platf::TOUCHPAD_BUTTON | platf::MISC_BUTTON
  ));

  EXPECT_FALSE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_PS, 0));
  EXPECT_FALSE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_UNKNOWN, platf::EDGE_PADDLE_BUTTONS));
  EXPECT_FALSE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_XBOX, platf::EDGE_PADDLE_BUTTONS));
  EXPECT_FALSE(platf::has_dualsense_edge_arrival_support(LI_CTYPE_NINTENDO, platf::EDGE_PADDLE_BUTTONS));
}
