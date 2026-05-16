/**
 * @file tests/unit/test_linux_virtual_display.cpp
 * @brief Test Linux virtual display pure helpers.
 */
#include "../tests_common.h"

#ifdef __linux__

#include <src/platform/linux/virtual_display.h>

TEST(LinuxVirtualDisplayBackendTest, ParsesSupportedBackendAliases) {
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("auto"), VDISPLAY::BACKEND::EVDI_PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("hybrid"), VDISPLAY::BACKEND::EVDI_PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("evdi-pipewire"), VDISPLAY::BACKEND::EVDI_PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("mutter"), VDISPLAY::BACKEND::MUTTER_PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("pipewire"), VDISPLAY::BACKEND::MUTTER_PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("evdi"), VDISPLAY::BACKEND::EVDI);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualDisplayBackend("evdi-kms"), VDISPLAY::BACKEND::EVDI);
}

TEST(LinuxVirtualDisplayBackendTest, RejectsUnsupportedBackendNames) {
  EXPECT_FALSE(VDISPLAY::parseLinuxVirtualDisplayBackend(""));
  EXPECT_FALSE(VDISPLAY::parseLinuxVirtualDisplayBackend("physical"));
  EXPECT_FALSE(VDISPLAY::parseLinuxVirtualDisplayBackend("kms"));
}

TEST(LinuxVirtualDisplayBackendTest, EnvironmentOverrideWinsWhenValid) {
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualDisplayBackend("evdi", "mutter"),
    VDISPLAY::BACKEND::MUTTER_PIPEWIRE
  );
}

TEST(LinuxVirtualDisplayBackendTest, InvalidEnvironmentOverrideFallsBackToConfig) {
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualDisplayBackend("evdi", "unknown"),
    VDISPLAY::BACKEND::EVDI
  );
}

TEST(LinuxVirtualDisplayBackendTest, InvalidConfigFallsBackToHybridDefault) {
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualDisplayBackend("unknown", nullptr),
    VDISPLAY::BACKEND::EVDI_PIPEWIRE
  );
}

#endif
