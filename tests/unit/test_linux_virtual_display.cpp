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

TEST(LinuxVirtualDisplayBackendTest, ParsesVirtualCaptureBackends) {
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualCaptureBackend("auto"), VDISPLAY::CAPTURE_BACKEND::AUTO);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualCaptureBackend("pipewire"), VDISPLAY::CAPTURE_BACKEND::PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualCaptureBackend("mutter-pipewire"), VDISPLAY::CAPTURE_BACKEND::PIPEWIRE);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualCaptureBackend("nvidia"), VDISPLAY::CAPTURE_BACKEND::NVIDIA);
  EXPECT_EQ(VDISPLAY::parseLinuxVirtualCaptureBackend("nvfbc"), VDISPLAY::CAPTURE_BACKEND::NVIDIA);
  EXPECT_FALSE(VDISPLAY::parseLinuxVirtualCaptureBackend("kms"));
}

TEST(LinuxVirtualDisplayBackendTest, ResolvesVirtualCaptureBackendPrecedence) {
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualCaptureBackend("pipewire", "nvidia"),
    VDISPLAY::CAPTURE_BACKEND::NVIDIA
  );
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualCaptureBackend("pipewire", "unknown"),
    VDISPLAY::CAPTURE_BACKEND::PIPEWIRE
  );
  EXPECT_EQ(
    VDISPLAY::resolveLinuxVirtualCaptureBackend("unknown", nullptr),
    VDISPLAY::CAPTURE_BACKEND::AUTO
  );
}

TEST(LinuxVirtualDisplayBackendTest, ParsesPipeWireDmaBufModes) {
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("auto"), VDISPLAY::PIPEWIRE_DMABUF::AUTO);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("off"), VDISPLAY::PIPEWIRE_DMABUF::OFF);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("disabled"), VDISPLAY::PIPEWIRE_DMABUF::OFF);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("0"), VDISPLAY::PIPEWIRE_DMABUF::OFF);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("force"), VDISPLAY::PIPEWIRE_DMABUF::FORCE);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("enabled"), VDISPLAY::PIPEWIRE_DMABUF::FORCE);
  EXPECT_EQ(VDISPLAY::parseLinuxPipeWireDmaBuf("1"), VDISPLAY::PIPEWIRE_DMABUF::FORCE);
  EXPECT_FALSE(VDISPLAY::parseLinuxPipeWireDmaBuf("maybe"));
}

TEST(LinuxVirtualDisplayBackendTest, ResolvesPipeWireDmaBufPrecedence) {
  EXPECT_EQ(
    VDISPLAY::resolveLinuxPipeWireDmaBuf("off", "force"),
    VDISPLAY::PIPEWIRE_DMABUF::FORCE
  );
  EXPECT_EQ(
    VDISPLAY::resolveLinuxPipeWireDmaBuf("off", "unknown"),
    VDISPLAY::PIPEWIRE_DMABUF::OFF
  );
  EXPECT_EQ(
    VDISPLAY::resolveLinuxPipeWireDmaBuf("unknown", nullptr),
    VDISPLAY::PIPEWIRE_DMABUF::OFF
  );
  EXPECT_EQ(
    VDISPLAY::resolveLinuxPipeWireDmaBuf("", nullptr),
    VDISPLAY::PIPEWIRE_DMABUF::OFF
  );
}

#endif
