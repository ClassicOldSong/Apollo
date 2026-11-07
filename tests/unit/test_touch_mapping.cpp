/**
 * @file tests/unit/test_touch_mapping.cpp
 * @brief Test touch/pen coordinate mapping from client stream to host display in src/input.*.
 */
#include "../tests_common.h"

#include <src/input.h>

namespace {
  constexpr float eps = 1e-3f;

  input::touch_port_t port(int stream_w, int stream_h, int display_w, int display_h) {
    // Display placed at an offset inside a larger environment to check offsets are not mixed into display coordinates
    return input::make_touch_port(100, 50, stream_w, stream_h, display_w, display_h, display_w + 200, display_h + 100);
  }

  void expect_maps(const input::touch_port_t &p, float x, float y, float hx, float hy) {
    auto [rx, ry] = input::client_to_display(p, x, y);
    EXPECT_NEAR(rx, hx, eps) << "client (" << x << ", " << y << ")";
    EXPECT_NEAR(ry, hy, eps) << "client (" << x << ", " << y << ")";
  }
}  // namespace

TEST(TouchMappingTest, SameResolution) {
  auto p = port(1920, 1080, 1920, 1080);
  EXPECT_NEAR(p.scale.scale_x, 1.0f, eps);
  EXPECT_NEAR(p.scale.scale_y, 1.0f, eps);
  expect_maps(p, 0.0f, 0.0f, 0.0f, 0.0f);
  expect_maps(p, 1.0f, 1.0f, 1920.0f, 1080.0f);
  expect_maps(p, 0.5f, 0.5f, 960.0f, 540.0f);
  expect_maps(p, 0.25f, 0.75f, 480.0f, 810.0f);
}

TEST(TouchMappingTest, SmallerClientSameAspect) {
  auto p = port(1280, 720, 1920, 1080);
  EXPECT_NEAR(p.scale.scale_x, 1.5f, eps);
  EXPECT_NEAR(p.scale.scale_y, 1.5f, eps);
  EXPECT_NEAR(p.scale.content_x, 0.0f, eps);
  EXPECT_NEAR(p.scale.content_y, 0.0f, eps);
  expect_maps(p, 0.0f, 0.0f, 0.0f, 0.0f);
  expect_maps(p, 1.0f, 0.0f, 1920.0f, 0.0f);
  expect_maps(p, 0.0f, 1.0f, 0.0f, 1080.0f);
  expect_maps(p, 1.0f, 1.0f, 1920.0f, 1080.0f);
  expect_maps(p, 0.5f, 0.5f, 960.0f, 540.0f);
}

TEST(TouchMappingTest, WiderClientPillarbox) {
  // 1920x1080 fits into 2400x1080 as 1920x1080 with 240px bars left and right
  auto p = port(2400, 1080, 1920, 1080);
  EXPECT_NEAR(p.scale.scale_x, 1.0f, eps);
  EXPECT_NEAR(p.scale.scale_y, 1.0f, eps);
  EXPECT_NEAR(p.scale.content_x, 240.0f, eps);
  EXPECT_NEAR(p.scale.content_y, 0.0f, eps);

  // Corners of the video area
  expect_maps(p, 240.0f / 2400.0f, 0.0f, 0.0f, 0.0f);
  expect_maps(p, 2160.0f / 2400.0f, 1.0f, 1920.0f, 1080.0f);
  expect_maps(p, 0.5f, 0.5f, 960.0f, 540.0f);

  // Touches on the bars clamp to the video edges
  expect_maps(p, 0.0f, 0.5f, 0.0f, 540.0f);
  expect_maps(p, 1.0f, 0.5f, 1920.0f, 540.0f);
}

TEST(TouchMappingTest, FourByThreeClientLetterbox) {
  // 1920x1080 fits into 1440x1080 as 1440x810 with 135px bars top and bottom
  auto p = port(1440, 1080, 1920, 1080);
  EXPECT_NEAR(p.scale.scale_x, 1920.0f / 1440.0f, eps);
  EXPECT_NEAR(p.scale.scale_y, 1920.0f / 1440.0f, eps);
  EXPECT_NEAR(p.scale.content_x, 0.0f, eps);
  EXPECT_NEAR(p.scale.content_y, 135.0f, eps);

  expect_maps(p, 0.0f, 135.0f / 1080.0f, 0.0f, 0.0f);
  expect_maps(p, 1.0f, 945.0f / 1080.0f, 1920.0f, 1080.0f);
  expect_maps(p, 0.5f, 0.5f, 960.0f, 540.0f);
  expect_maps(p, 0.5f, 0.0f, 960.0f, 0.0f);
  expect_maps(p, 0.5f, 1.0f, 960.0f, 1080.0f);
}

TEST(TouchMappingTest, OutOfRangeInputIsClamped) {
  auto p = port(1280, 720, 1920, 1080);
  expect_maps(p, -0.5f, 2.0f, 0.0f, 1080.0f);
}

TEST(TouchMappingTest, PlatformPortContract) {
  auto p = port(1280, 720, 1920, 1080);
  auto abs_port = input::platform_touch_port(p);
  EXPECT_EQ(abs_port.offset_x, 100);
  EXPECT_EQ(abs_port.offset_y, 50);
  EXPECT_EQ(abs_port.width, 2120);
  EXPECT_EQ(abs_port.height, 1180);

  auto display_port = input::platform_display_port(p);
  EXPECT_EQ(display_port.offset_x, 100);
  EXPECT_EQ(display_port.offset_y, 50);
  EXPECT_EQ(display_port.width, 1920);
  EXPECT_EQ(display_port.height, 1080);
}

TEST(TouchMappingTest, ContactAreaScalesIntoHostPixels) {
  auto p = port(1280, 720, 1920, 1080);
  auto scalar = input::client_contact_area_scalar(p);
  EXPECT_NEAR(scalar.first, 1920.0f, eps);
  EXPECT_NEAR(scalar.second, 1080.0f, eps);

  // A horizontal ellipse covering 10% of the client width and 10% of its height
  auto area = input::scale_client_contact_area({0.1f, 0.1f}, 0, scalar);
  EXPECT_NEAR(area.first, 192.0f, 0.01f);
  EXPECT_NEAR(area.second, 108.0f, 0.01f);

  // Pillarboxed: the contact area follows the display scale, not the environment size
  auto wide = port(2400, 1080, 1920, 1080);
  auto wide_area = input::scale_client_contact_area({0.1f, 0.1f}, 0, input::client_contact_area_scalar(wide));
  EXPECT_NEAR(wide_area.first, 240.0f, 0.01f);
  EXPECT_NEAR(wide_area.second, 108.0f, 0.01f);
}

TEST(TouchMappingTest, InvalidSizesAreRejected) {
  EXPECT_FALSE(input::touch_port_t {});
  EXPECT_FALSE(port(0, 720, 1920, 1080));
  EXPECT_FALSE(port(1280, 720, 0, 1080));
  EXPECT_TRUE(port(1280, 720, 1920, 1080));
}
