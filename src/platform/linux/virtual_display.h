/**
 * @file src/platform/linux/virtual_display.h
 * @brief [Experimental] Linux virtual display via DRM EDID override.
 */
#pragma once

// standard includes
#include <cstdint>
#include <string>

namespace linux_vdisplay {

  enum class status_e {
    OK = 0,
    NOT_SUPPORTED = -1,
    NO_CONNECTOR = -2,
    EDID_WRITE_FAILED = -3,
    REPROBE_FAILED = -4,
    COMPOSITOR_TIMEOUT = -5,
  };

  struct connector_info_t {
    std::string card_path;
    int card_index;
    std::uint32_t connector_id;
    std::uint32_t connector_type;
    std::uint32_t connector_index;
    std::string sysfs_name;
    std::string debugfs_edid_path;
    std::string sysfs_status_path;
    std::string debugfs_hotplug_path;
  };

  bool is_supported();
  status_e create(std::string &out_display_name, int width, int height, int fps_millihertz, const std::string &client_id);
  status_e remove();
  bool is_active();
  int get_kms_index();

}  // namespace linux_vdisplay
