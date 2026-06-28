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

  const std::uint16_t low_buttons = packet.buttonFlags;
  const std::uint32_t high_buttons = static_cast<std::uint16_t>(packet.buttonFlags2);

  EXPECT_EQ(low_buttons | (high_buttons << 16), paddles);
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

    const std::uint16_t low_buttons = packet.buttonFlags;
    const std::uint32_t high_buttons = static_cast<std::uint16_t>(packet.buttonFlags2);

    EXPECT_EQ(low_buttons | (high_buttons << 16), test_case.flag);
  }
}

TEST(GamepadProtocolTests, ControllerArrivalCanAdvertiseAllPaddleFlags) {
  constexpr std::uint32_t paddles =
    PADDLE1_FLAG | PADDLE2_FLAG | PADDLE3_FLAG | PADDLE4_FLAG;

  SS_CONTROLLER_ARRIVAL_PACKET packet {};
  packet.supportedButtonFlags = paddles | TOUCHPAD_FLAG | MISC_FLAG;

  EXPECT_EQ(packet.supportedButtonFlags & paddles, paddles);
}

TEST(GamepadProtocolTests, EdgeModeRequiresCompletePaddleArrivalMask) {
  EXPECT_TRUE(platf::has_all_edge_paddle_buttons(platf::EDGE_PADDLE_BUTTONS));
  EXPECT_TRUE(platf::has_all_edge_paddle_buttons(
    platf::EDGE_PADDLE_BUTTONS | platf::TOUCHPAD_BUTTON | platf::MISC_BUTTON
  ));

  EXPECT_FALSE(platf::has_all_edge_paddle_buttons(0));
  EXPECT_FALSE(platf::has_all_edge_paddle_buttons(platf::PADDLE1 | platf::PADDLE2 | platf::PADDLE3));
}
