/**
 * @file src/platform/linux/virtual_display.cpp
 * @brief [Experimental] Linux virtual display via DRM EDID override.
 */
// standard includes
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <thread>
#include <unistd.h>

// lib includes
#include <boost/process/v1/args.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/pipe.hpp>
#include <boost/process/v1/search_path.hpp>

// platform includes
#include <xf86drm.h>
#include <xf86drmMode.h>

// local includes
#include "misc.h"
#include "src/config.h"
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/utility.h"
#include "virtual_display.h"

using namespace std::literals;
namespace bp = boost::process::v1;
namespace fs = std::filesystem;

namespace linux_vdisplay {

  struct cvt_timing_t {
    uint32_t pixel_clock_khz;
    uint16_t h_active, h_blanking, h_front_porch, h_sync_width;
    uint16_t v_active, v_blanking, v_front_porch, v_sync_width;
    uint16_t h_total, v_total;
  };

  // CVT spec according to VESA for vsync width
  static int
    cvt_vsync_width(int width, int height) {
    int g = std::gcd(width, height);
    int aw = width / g, ah = height / g;
    if (aw == 4 && ah == 3) {
      return 4;
    }
    if (aw == 16 && ah == 9) {
      return 5;
    }
    if (aw == 16 && ah == 10) {
      return 6;
    }
    if (aw == 5 && ah == 4) {
      return 7;
    }
    if (aw == 15 && ah == 9) {
      return 7;
    }
    return 10;
  }

  static cvt_timing_t
    calculate_cvt_rb(int width, int height, int refresh_hz) {
    // Fixed constants from VESA CVT Reduced Blanking v1 spec
    constexpr int RB_H_BLANK = 160;
    constexpr int RB_H_SYNC = 32;
    constexpr int RB_H_FRONT_PORCH = 48;
    constexpr int RB_V_FRONT_PORCH = 3;
    constexpr double RB_MIN_VBLANK_US = 460.0;

    cvt_timing_t t {};
    t.h_active = static_cast<uint16_t>(width & ~7);  // EDID DTD requires multiple of 8
    t.h_blanking = RB_H_BLANK;
    t.h_front_porch = RB_H_FRONT_PORCH;
    t.h_sync_width = RB_H_SYNC;
    t.h_total = t.h_active + RB_H_BLANK;

    t.v_active = static_cast<uint16_t>(height);
    t.v_front_porch = RB_V_FRONT_PORCH;
    t.v_sync_width = static_cast<uint16_t>(cvt_vsync_width(width, height));

    double h_period_us = (1e6 / refresh_hz - RB_MIN_VBLANK_US) / (height + RB_V_FRONT_PORCH);
    int vbi_lines = static_cast<int>(std::ceil(RB_MIN_VBLANK_US / h_period_us)) + 1;
    int min_vbi = t.v_sync_width + t.v_front_porch + 1;
    if (vbi_lines < min_vbi) {
      vbi_lines = min_vbi;
    }

    t.v_blanking = static_cast<uint16_t>(vbi_lines);
    t.v_total = t.v_active + t.v_blanking;
    t.pixel_clock_khz = static_cast<uint32_t>(
      static_cast<uint64_t>(t.h_total) * t.v_total * refresh_hz / 1000
    );

    return t;
  }

  // Pack timing into EDID 1.4 Detailed Timing Descriptor (18 bytes)
  static void
    build_dtd(uint8_t dtd[18], const cvt_timing_t &t, int h_mm, int v_mm) {
    uint16_t pclk_10khz = static_cast<uint16_t>(t.pixel_clock_khz / 10);
    dtd[0] = pclk_10khz & 0xFF;
    dtd[1] = (pclk_10khz >> 8) & 0xFF;
    dtd[2] = t.h_active & 0xFF;
    dtd[3] = t.h_blanking & 0xFF;
    dtd[4] = static_cast<uint8_t>(((t.h_active >> 8) & 0x0F) << 4 | ((t.h_blanking >> 8) & 0x0F));
    dtd[5] = t.v_active & 0xFF;
    dtd[6] = t.v_blanking & 0xFF;
    dtd[7] = static_cast<uint8_t>(((t.v_active >> 8) & 0x0F) << 4 | ((t.v_blanking >> 8) & 0x0F));
    dtd[8] = t.h_front_porch & 0xFF;
    dtd[9] = t.h_sync_width & 0xFF;
    dtd[10] = static_cast<uint8_t>(((t.v_front_porch & 0x0F) << 4) | (t.v_sync_width & 0x0F));
    dtd[11] = static_cast<uint8_t>(
      ((t.h_front_porch >> 8) & 0x03) << 6 |
      ((t.h_sync_width >> 8) & 0x03) << 4 |
      ((t.v_front_porch >> 4) & 0x03) << 2 |
      ((t.v_sync_width >> 4) & 0x03)
    );
    dtd[12] = h_mm & 0xFF;
    dtd[13] = v_mm & 0xFF;
    dtd[14] = static_cast<uint8_t>(((h_mm >> 8) & 0x0F) << 4 | ((v_mm >> 8) & 0x0F));
    dtd[15] = 0;
    dtd[16] = 0;
    // Non-interlaced, digital separate, H+ V- (CVT-RB)
    dtd[17] = 0x1A;
  }

  static uint8_t
    edid_checksum(const uint8_t *block, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len - 1; i++) {
      sum += block[i];
    }
    return static_cast<uint8_t>((256 - sum) % 256);
  }

  static void
    build_monitor_name(uint8_t desc[18], int width, int height) {
    desc[0] = desc[1] = desc[2] = 0x00;
    desc[3] = 0xFC;  // EDID descriptor tag: monitor name
    desc[4] = 0x00;

    char name[14];
    int n = snprintf(name, sizeof(name), "APL %dx%d", width, height);
    if (n < 0 || n >= 14) {
      snprintf(name, sizeof(name), "APL Virtual");
    }

    int i = 0;
    for (; i < 13 && name[i]; i++) {
      desc[5 + i] = static_cast<uint8_t>(name[i]);
    }
    if (i < 13) {
      desc[5 + i] = 0x0A;  // EDID requires LF terminator after name
      i++;
    }
    for (; i < 13; i++) {
      desc[5 + i] = 0x20;  // pad with spaces per EDID spec
    }
  }

  static void
    build_range_limits(uint8_t desc[18], int min_v, int max_v, int max_pclk_mhz, const cvt_timing_t &t) {
    desc[0] = desc[1] = desc[2] = 0x00;
    desc[3] = 0xFD;  // EDID descriptor tag: monitor range limits
    desc[4] = 0x00;
    desc[5] = static_cast<uint8_t>(std::clamp(min_v, 1, 255));
    desc[6] = static_cast<uint8_t>(std::clamp(max_v, 1, 255));

    int h_rate_khz = static_cast<int>(t.pixel_clock_khz / t.h_total);
    desc[7] = static_cast<uint8_t>(std::clamp(h_rate_khz - 1, 1, 255));
    desc[8] = static_cast<uint8_t>(std::clamp(h_rate_khz + 1, 1, 255));

    desc[9] = static_cast<uint8_t>(std::clamp((max_pclk_mhz + 9) / 10, 1, 255));
    desc[10] = 0x01;  // default GTF timing type
    desc[11] = 0x0A;  // LF terminator
    for (int i = 12; i < 18; i++) {
      desc[i] = 0x20;  // space padding
    }
  }

  // We need to give each client a unique hash ID for the EDID serial.
  // This lets compositors actually remember the vdisplay settings per client.
  // Uses FNV-1a.
  static uint32_t
    hash_client_id(const std::string &client_id) {
    uint32_t hash = 0x811c9dc5;
    for (auto c : client_id) {
      hash ^= static_cast<uint8_t>(c);
      hash *= 0x01000193;
    }
    return hash;
  }

  static std::array<uint8_t, 256>
    generate_edid(int width, int height, int refresh_hz, const std::string &client_id) {
    std::array<uint8_t, 256> edid {};

    auto t = calculate_cvt_rb(width, height, refresh_hz);

    // DTD pixel clock field is 16-bit (units of 10 kHz), clamp refresh if it overflows
    uint32_t pclk_10khz = t.pixel_clock_khz / 10;
    if (pclk_10khz > 65535) {
      int max_refresh = static_cast<int>(65535ULL * 10000 / (static_cast<uint64_t>(t.h_total) * t.v_total));
      BOOST_LOG(warning) << "[Experimental] Pixel clock overflow for "sv << width << "x"sv << height
                         << "@"sv << refresh_hz << "Hz, clamping to "sv << max_refresh << "Hz"sv;
      refresh_hz = max_refresh;
      t = calculate_cvt_rb(width, height, refresh_hz);
    }

    // Physical size in mm since EDID must report it.
    int h_mm = static_cast<int>(width * 25.4 / 96.0 + 0.5);
    int v_mm = static_cast<int>(height * 25.4 / 96.0 + 0.5);

    // --- Base block (bytes 0-127) ---
    // Header
    edid[0] = 0x00;
    edid[1] = edid[2] = edid[3] = edid[4] = edid[5] = edid[6] = 0xFF;
    edid[7] = 0x00;

    // Manufacturer "APL": A=1, P=16, L=12 → ((1<<10)|(16<<5)|12) = 0x060C
    edid[8] = 0x06;
    edid[9] = 0x0C;
    // Product code
    edid[10] = 0x01;
    edid[11] = 0x00;
    // Serial — hash of client ID for per-client display identity
    uint32_t serial = client_id.empty() ? 1 : hash_client_id(client_id);
    edid[12] = static_cast<uint8_t>(serial & 0xFF);
    edid[13] = static_cast<uint8_t>((serial >> 8) & 0xFF);
    edid[14] = static_cast<uint8_t>((serial >> 16) & 0xFF);
    edid[15] = static_cast<uint8_t>((serial >> 24) & 0xFF);
    // Week 1, year 2024 (2024-1990=34)
    edid[16] = 0x01;
    edid[17] = 0x22;
    // EDID 1.4
    edid[18] = 0x01;
    edid[19] = 0x04;
    // Digital, 8-bit, DisplayPort
    edid[20] = 0xA5;
    // Physical size in cm
    edid[21] = static_cast<uint8_t>(std::clamp(h_mm / 10, 1, 255));
    edid[22] = static_cast<uint8_t>(std::clamp(v_mm / 10, 1, 255));
    // Gamma 2.2 — encoded as (2.2 * 100) - 100 = 120 = 0x78
    edid[23] = 0x78;
    // Features bitmap: RGB color, preferred timing is native, continuous frequency support
    edid[24] = 0x3B;

    // Standard sRGB chromaticity coordinates (CIE 1931 color primaries)
    static constexpr uint8_t SRGB_CHROMA[] = {
      0x0C,
      0xC1,
      0xAF,
      0x4F,
      0x40,
      0xAB,
      0x25,
      0x14,
      0x50,
      0x54
    };
    std::copy(std::begin(SRGB_CHROMA), std::end(SRGB_CHROMA), &edid[25]);

    // Established timings — none
    edid[35] = edid[36] = edid[37] = 0x00;

    // Standard timings — 0x0101 is the EDID "unused slot" marker
    for (int i = 38; i < 54; i += 2) {
      edid[i] = 0x01;
      edid[i + 1] = 0x01;
    }

    // DTD1: preferred mode (target resolution)
    build_dtd(&edid[54], t, h_mm, v_mm);

    // DTD2: 1080p@60 fallback (if primary isn't already 1080p@60)
    if (width != 1920 || height != 1080 || refresh_hz != 60) {
      auto t_fallback = calculate_cvt_rb(1920, 1080, 60);
      int fb_h_mm = static_cast<int>(1920 * 25.4 / 96.0 + 0.5);
      int fb_v_mm = static_cast<int>(1080 * 25.4 / 96.0 + 0.5);
      build_dtd(&edid[72], t_fallback, fb_h_mm, fb_v_mm);
    } else {
      // Serial number descriptor as placeholder
      edid[72] = edid[73] = edid[74] = 0x00;
      edid[75] = 0xFF;
      edid[76] = 0x00;
      edid[77] = '0';
      edid[78] = '0';
      edid[79] = '0';
      edid[80] = '1';
      edid[81] = 0x0A;
      for (int i = 82; i < 90; i++) {
        edid[i] = 0x20;
      }
    }

    // Monitor name
    build_monitor_name(&edid[90], width, height);

    // Range limits
    int min_v = std::min(refresh_hz - 1, 24);
    int max_v = std::max(refresh_hz, 144);
    int max_pclk_mhz = static_cast<int>(t.pixel_clock_khz / 1000) + 10;
    build_range_limits(&edid[108], min_v, max_v, max_pclk_mhz, t);

    // Extension count
    edid[126] = 0x01;
    // Checksum
    edid[127] = edid_checksum(&edid[0], 128);

    // --- CTA extension block (bytes 128-255) ---
    edid[128] = 0x02;  // CTA extension tag
    edid[129] = 0x03;  // CTA revision 3

    // Data blocks start at byte 132 (offset 4 within extension)
    int cta_offset = 4;

    // Video Data Block: tag 0x40 | length
    // VIC 16 = 1080p60, VIC 4 = 720p60, VIC 1 = 640x480p60
    edid[128 + cta_offset] = 0x43;  // tag 2, length 3
    edid[128 + cta_offset + 1] = 0x90;  // VIC 16 with native bit set
    edid[128 + cta_offset + 2] = 0x04;  // VIC 4
    edid[128 + cta_offset + 3] = 0x01;  // VIC 1
    cta_offset += 4;

    // Audio Data Block: 2ch LPCM, 48kHz, 16-bit
    edid[128 + cta_offset] = 0x23;  // tag 1, length 3
    edid[128 + cta_offset + 1] = 0x09;  // LPCM, 2 channels
    edid[128 + cta_offset + 2] = 0x07;  // 32/44.1/48 kHz
    edid[128 + cta_offset + 3] = 0x01;  // 16-bit
    cta_offset += 4;

    // Speaker Allocation: FL + FR
    edid[128 + cta_offset] = 0x83;  // tag 4, length 3
    edid[128 + cta_offset + 1] = 0x01;  // FL + FR
    edid[128 + cta_offset + 2] = 0x00;
    edid[128 + cta_offset + 3] = 0x00;
    cta_offset += 4;

    // DTD offset in CTA block
    edid[130] = static_cast<uint8_t>(cta_offset);
    // CTA features: underscan supported (0x80) + basic audio (0x40), no YCbCr
    edid[131] = 0xC0;

    // DTD in CTA block: same as primary
    if (cta_offset + 18 <= 127) {
      build_dtd(&edid[128 + cta_offset], t, h_mm, v_mm);
      cta_offset += 18;
    }

    // Pad remainder with zeros (already zero from array init)

    // CTA checksum
    edid[255] = edid_checksum(&edid[128], 128);

    return edid;
  }

  struct active_state_t {
    connector_info_t connector;
    bool edid_written = false;
    bool reprobed = false;
  };

  static std::optional<active_state_t> active_state;

  static fs::path
    state_file_path() {
    return platf::appdata() / "vdisplay.state";
  }

  static void
    save_state(const connector_info_t &conn) {
    std::error_code ec;
    std::ofstream f(state_file_path(), std::ios::trunc);
    if (f) {
      f << conn.debugfs_edid_path << '\n'
        << conn.sysfs_status_path << '\n'
        << conn.debugfs_hotplug_path << '\n'
        << conn.sysfs_name << '\n';
    }
  }

  static void
    clear_state() {
    std::error_code ec;
    fs::remove(state_file_path(), ec);
  }

  using version_ptr = util::safe_ptr<drmVersion, drmFreeVersion>;
  using res_ptr = util::safe_ptr<drmModeRes, drmModeFreeResources>;
  using conn_ptr = util::safe_ptr<drmModeConnector, drmModeFreeConnector>;
  using enc_ptr = util::safe_ptr<drmModeEncoder, drmModeFreeEncoder>;

  static std::string
    connector_type_name(std::uint32_t type) {
    switch (type) {
      case DRM_MODE_CONNECTOR_VGA:
        return "VGA";
      case DRM_MODE_CONNECTOR_DVII:
        return "DVI-I";
      case DRM_MODE_CONNECTOR_DVID:
        return "DVI-D";
      case DRM_MODE_CONNECTOR_DVIA:
        return "DVI-A";
      case DRM_MODE_CONNECTOR_Composite:
        return "Composite";
      case DRM_MODE_CONNECTOR_SVIDEO:
        return "SVIDEO";
      case DRM_MODE_CONNECTOR_LVDS:
        return "LVDS";
      case DRM_MODE_CONNECTOR_Component:
        return "Component";
      case DRM_MODE_CONNECTOR_9PinDIN:
        return "DIN";
      case DRM_MODE_CONNECTOR_DisplayPort:
        return "DP";
      case DRM_MODE_CONNECTOR_HDMIA:
        return "HDMI-A";
      case DRM_MODE_CONNECTOR_HDMIB:
        return "HDMI-B";
      case DRM_MODE_CONNECTOR_TV:
        return "TV";
      case DRM_MODE_CONNECTOR_eDP:
        return "eDP";
      case DRM_MODE_CONNECTOR_VIRTUAL:
        return "Virtual";
      case DRM_MODE_CONNECTOR_DSI:
        return "DSI";
      case DRM_MODE_CONNECTOR_DPI:
        return "DPI";
      case DRM_MODE_CONNECTOR_WRITEBACK:
        return "Writeback";
      case DRM_MODE_CONNECTOR_SPI:
        return "SPI";
#ifdef DRM_MODE_CONNECTOR_USB
      case DRM_MODE_CONNECTOR_USB:
        return "USB";
#endif
      default:
        return "Unknown" + std::to_string(type);
    }
  }

  static bool
    card_is_nvidia(int fd) {
    version_ptr ver {drmGetVersion(fd)};
    return ver && ver->name && strncmp(ver->name, "nvidia-drm", 10) == 0;
  }

  // Arbitrary but deterministic ordering for auto-selection when multiple connectors are available
  static int
    connector_type_priority(std::uint32_t type) {
    switch (type) {
      case DRM_MODE_CONNECTOR_HDMIA:
        return 0;
      case DRM_MODE_CONNECTOR_DisplayPort:
        return 1;
      case DRM_MODE_CONNECTOR_HDMIB:
        return 2;
      case DRM_MODE_CONNECTOR_DVID:
        return 3;
      case DRM_MODE_CONNECTOR_DVII:
        return 4;
      case DRM_MODE_CONNECTOR_VGA:
        return 5;
      default:
        return 99;
    }
  }

  static std::vector<connector_info_t>
    find_disconnected_connectors() {
    std::vector<connector_info_t> result;

    if (!fs::exists("/dev/dri"sv)) {
      return result;
    }

    for (auto &entry : fs::directory_iterator {"/dev/dri"sv}) {
      auto filename = entry.path().filename().string();
      if (filename.substr(0, 4) != "card"sv) {
        continue;
      }

      int card_index = -1;
      try {
        card_index = std::stoi(filename.substr(4));
      } catch (...) {
        continue;
      }

      int fd = open(entry.path().c_str(), O_RDWR);
      if (fd < 0) {
        continue;
      }

      auto close_fd = util::fail_guard([fd]() {
        close(fd);
      });

      // Nvidia not tested but may have issues with EDID override + NVKMS, so log a warning if detected
      if (card_is_nvidia(fd)) {
        BOOST_LOG(warning) << "[Experimental] NVIDIA card detected ("sv << filename << ") — EDID override may cause issues"sv;
      }

      res_ptr resources {drmModeGetResources(fd)};
      if (!resources) {
        continue;
      }

      // Track per-type index to match kernel sysfs naming
      std::map<std::uint32_t, std::uint32_t> type_count;

      for (int i = 0; i < resources->count_connectors; i++) {
        conn_ptr conn {drmModeGetConnector(fd, resources->connectors[i])};
        if (!conn) {
          continue;
        }

        // Increment before the connected check — kernel numbers ALL connectors of a type, not just disconnected ones
        auto index = ++type_count[conn->connector_type];

        if (conn->connection == DRM_MODE_CONNECTED) {
          continue;
        }

        auto type_name = connector_type_name(conn->connector_type);
        auto connector_name = type_name + "-" + std::to_string(index);
        auto sysfs_name = filename + "-" + connector_name;

        auto debugfs_dir = "/sys/kernel/debug/dri/"s + std::to_string(card_index) + "/" + connector_name;
        auto debugfs_path = debugfs_dir + "/edid_override";
        auto hotplug_path = debugfs_dir + "/trigger_hotplug";
        auto status_path = "/sys/class/drm/"s + sysfs_name + "/status";

        std::error_code ec;
        if (!fs::exists(status_path, ec) || ec) {
          BOOST_LOG(debug) << "[Experimental] sysfs path missing: "sv << status_path;
          continue;
        }

        result.push_back(connector_info_t {
          entry.path().string(),
          card_index,
          conn->connector_id,
          conn->connector_type,
          index,
          sysfs_name,
          debugfs_path,
          status_path,
          hotplug_path,
        });

        BOOST_LOG(debug) << "[Experimental] Found disconnected connector: "sv << sysfs_name;
      }
    }

    return result;
  }

  static connector_info_t
    select_best(const std::vector<connector_info_t> &candidates) {
    auto best = candidates.begin();
    for (auto it = candidates.begin(); it != candidates.end(); ++it) {
      if (connector_type_priority(it->connector_type) < connector_type_priority(best->connector_type)) {
        best = it;
      }
    }
    return *best;
  }

  // Prefer helper binary next to our own executable, fall back to PATH lookup
  static boost::filesystem::path
    find_helper() {
    std::error_code ec;
    auto self = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
      auto sibling = self.parent_path() / "apollo-vdisplay-helper";
      if (fs::exists(sibling, ec)) {
        return boost::filesystem::path(sibling.string());
      }
    }
    return bp::search_path("apollo-vdisplay-helper");
  }

  // Instead of doing debugfs edits here which would require cap_dac_override caps
  // for the entire process we offload this to a helper bin.
  //
  // See vdisplay_helper.cpp
  static bool
    run_helper(const std::vector<std::string> &args, const uint8_t *stdin_data = nullptr, size_t stdin_len = 0) {
    auto helper = find_helper();
    if (helper.empty()) {
      BOOST_LOG(error) << "[Experimental] apollo-vdisplay-helper not found"sv;
      return false;
    }

    try {
      if (stdin_data && stdin_len > 0) {
        bp::pipe stdin_pipe;
        bp::child proc(helper, bp::args(args), bp::std_in<stdin_pipe, bp::std_out> bp::null, bp::std_err > bp::null);
        stdin_pipe.write(reinterpret_cast<const char *>(stdin_data), static_cast<int>(stdin_len));
        stdin_pipe.close();
        proc.wait();
        return proc.exit_code() == 0;
      }

      bp::child proc(helper, bp::args(args), bp::std_in<bp::null, bp::std_out> bp::null, bp::std_err > bp::null);
      proc.wait();
      return proc.exit_code() == 0;
    } catch (const std::exception &e) {
      BOOST_LOG(error) << "[Experimental] Helper exec failed: "sv << e.what();
      return false;
    }
  }

  static bool
    helper_check_path(const std::string &path) {
    return run_helper({"check-path", path});
  }

  static bool
    write_edid(const connector_info_t &conn, const uint8_t *edid_data, size_t edid_len) {
    if (!run_helper({"write-edid", conn.debugfs_edid_path}, edid_data, edid_len)) {
      BOOST_LOG(error) << "[Experimental] Helper failed to write EDID to "sv << conn.debugfs_edid_path;
      return false;
    }
    BOOST_LOG(info) << "[Experimental] EDID written to "sv << conn.debugfs_edid_path;
    return true;
  }

  static bool
    clear_edid(const connector_info_t &conn) {
    if (!run_helper({"clear-edid", conn.debugfs_edid_path})) {
      BOOST_LOG(warning) << "[Experimental] Helper failed to clear EDID on "sv << conn.sysfs_name;
      return false;
    }
    BOOST_LOG(info) << "[Experimental] EDID override cleared on "sv << conn.sysfs_name;
    return true;
  }

  static bool
    force_status(const connector_info_t &conn, const char *value) {
    if (!run_helper({"write-string", conn.sysfs_status_path, value})) {
      BOOST_LOG(error) << "[Experimental] Helper failed to set status on "sv << conn.sysfs_name;
      return false;
    }
    BOOST_LOG(info) << "[Experimental] Connector status set to '"sv << value << "' on "sv << conn.sysfs_name;
    return true;
  }

  static bool
    trigger_hotplug(const connector_info_t &conn) {
    if (conn.debugfs_hotplug_path.empty()) {
      return false;
    }
    if (!run_helper({"write-string", conn.debugfs_hotplug_path, "1"})) {
      BOOST_LOG(warning) << "[Experimental] Helper failed to trigger hotplug on "sv << conn.sysfs_name;
      return false;
    }
    BOOST_LOG(info) << "[Experimental] Hotplug triggered on "sv << conn.sysfs_name;
    return true;
  }

  // Poll until the compositor assigns a CRTC to the connector (meaning it recognized the new display)
  static bool
    wait_for_connector_active(const connector_info_t &conn, int timeout_ms) {
    int fd = open(conn.card_path.c_str(), O_RDWR);
    if (fd < 0) {
      return false;
    }
    auto close_fd = util::fail_guard([fd]() {
      close(fd);
    });

    constexpr int poll_interval_ms = 200;
    int elapsed = 0;

    while (elapsed < timeout_ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
      elapsed += poll_interval_ms;

      conn_ptr drm_conn {drmModeGetConnector(fd, conn.connector_id)};
      if (!drm_conn) {
        continue;
      }

      if (drm_conn->connection != DRM_MODE_CONNECTED) {
        BOOST_LOG(debug) << "[Experimental] Waiting for connector... ("sv << elapsed << "ms)"sv;
        continue;
      }

      // Connected — check if a CRTC is assigned (compositor picked it up)
      if (drm_conn->encoder_id) {
        enc_ptr enc {drmModeGetEncoder(fd, drm_conn->encoder_id)};
        if (enc && enc->crtc_id) {
          BOOST_LOG(info) << "[Experimental] Connector active with CRTC after "sv << elapsed << "ms"sv;
          return true;
        }
      }

      // Connected but no CRTC yet — compositor may still be processing
      BOOST_LOG(debug) << "[Experimental] Connector connected, waiting for CRTC assignment... ("sv << elapsed << "ms)"sv;
    }

    BOOST_LOG(warning) << "[Experimental] Timed out waiting for compositor to pick up virtual display ("sv << timeout_ms << "ms)"sv;
    return false;
  }

  // X11 may needs an explicit xrandr call to activate the new output; Wayland compositors do it automatically (untested)
  static void
    configure_xrandr(const connector_info_t &conn) {
    auto type_name = connector_type_name(conn.connector_type);
    auto output_name = type_name + "-" + std::to_string(conn.connector_index);

    auto cmd = "xrandr --output "s + output_name + " --auto 2>&1";
    BOOST_LOG(info) << "[Experimental] Configuring X11 output: "sv << cmd;

    auto ret = system(cmd.c_str());
    if (ret != 0) {
      BOOST_LOG(warning) << "[Experimental] xrandr returned "sv << ret;
    }
  }

  // Walk DRM planes the same way kmsgrab does so the index we return matches what it captures
  static int
    find_kms_index(const connector_info_t &conn) {
    int fd = open(conn.card_path.c_str(), O_RDWR);
    if (fd < 0) {
      return -1;
    }
    auto close_fd = util::fail_guard([fd]() {
      close(fd);
    });

    // Find which CRTC is assigned to our connector
    res_ptr resources {drmModeGetResources(fd)};
    if (!resources) {
      return -1;
    }

    std::uint32_t target_crtc = 0;
    for (int i = 0; i < resources->count_connectors; i++) {
      conn_ptr c {drmModeGetConnector(fd, resources->connectors[i])};
      if (!c || c->connector_id != conn.connector_id) {
        continue;
      }
      if (c->encoder_id) {
        enc_ptr enc {drmModeGetEncoder(fd, c->encoder_id)};
        if (enc) {
          target_crtc = enc->crtc_id;
        }
      }
      break;
    }

    if (!target_crtc) {
      return -1;
    }

    // Iterate planes same way kmsgrab does to find the index
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    auto plane_res = drmModeGetPlaneResources(fd);
    if (!plane_res) {
      return -1;
    }

    int monitor = 0;
    for (std::uint32_t i = 0; i < plane_res->count_planes; i++) {
      auto plane = drmModeGetPlane(fd, plane_res->planes[i]);
      if (!plane) {
        continue;
      }

      bool has_fb = plane->fb_id != 0;
      auto plane_crtc = plane->crtc_id;
      drmModeFreePlane(plane);

      if (!has_fb) {
        continue;
      }

      // Skip cursor planes
      auto props = drmModeObjectGetProperties(fd, plane_res->planes[i], DRM_MODE_OBJECT_PLANE);
      if (props) {
        bool is_cursor = false;
        for (std::uint32_t j = 0; j < props->count_props; j++) {
          auto prop = drmModeGetProperty(fd, props->props[j]);
          if (prop) {
            if (std::string_view(prop->name) == "type"sv && props->prop_values[j] == DRM_PLANE_TYPE_CURSOR) {
              is_cursor = true;
            }
            drmModeFreeProperty(prop);
          }
          if (is_cursor) {
            break;
          }
        }
        drmModeFreeObjectProperties(props);
        if (is_cursor) {
          continue;
        }
      }

      if (plane_crtc == target_crtc) {
        drmModeFreePlaneResources(plane_res);
        BOOST_LOG(info) << "[Experimental] Virtual display is at KMS monitor index "sv << monitor;
        return monitor;
      }
      ++monitor;
    }

    drmModeFreePlaneResources(plane_res);
    return -1;
  }

  bool
    is_supported() {
    std::error_code ec;

    if (!helper_check_path("/sys/kernel/debug/dri")) {
      BOOST_LOG(debug) << "[Experimental] debugfs DRI not available"sv;
      return false;
    }

    if (!fs::exists("/dev/dri"sv, ec) || ec) {
      return false;
    }

    for (auto &entry : fs::directory_iterator {"/dev/dri"sv, ec}) {
      auto filename = entry.path().filename().string();
      if (filename.substr(0, 4) != "card"sv) {
        continue;
      }

      int fd = open(entry.path().c_str(), O_RDWR);
      if (fd < 0) {
        continue;
      }

      close(fd);
      return true;
    }

    return false;
  }

  void
    recover_crash_state() {
    auto path = state_file_path();
    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
      return;
    }

    BOOST_LOG(warning) << "[Experimental] Found stale vdisplay state file — recovering from previous crash"sv;

    std::ifstream f(path);
    std::string edid_path, status_path, hotplug_path, sysfs_name;
    if (!std::getline(f, edid_path) || !std::getline(f, status_path) || !std::getline(f, hotplug_path) || !std::getline(f, sysfs_name)) {
      BOOST_LOG(warning) << "[Experimental] Corrupt state file, removing"sv;
      clear_state();
      return;
    }

    connector_info_t conn {};
    conn.debugfs_edid_path = edid_path;
    conn.sysfs_status_path = status_path;
    conn.debugfs_hotplug_path = hotplug_path;
    conn.sysfs_name = sysfs_name;

    clear_edid(conn);
    force_status(conn, "detect");
    trigger_hotplug(conn);

    clear_state();
    BOOST_LOG(info) << "[Experimental] Crash recovery complete — cleared stale EDID on "sv << sysfs_name;
  }

  static status_e
    create_impl(std::string &out_display_name, int width, int height, int fps_millihertz, const std::string &client_id) {
    if (!is_supported()) {
      BOOST_LOG(info) << "[Experimental] Virtual display not supported on this system"sv;
      return status_e::NOT_SUPPORTED;
    }

    if (active_state) {
      remove();
    }

    auto candidates = find_disconnected_connectors();
    if (candidates.empty()) {
      BOOST_LOG(warning) << "[Experimental] No disconnected GPU connectors found"sv;
      return status_e::NO_CONNECTOR;
    }

    connector_info_t conn;
    const auto &preferred = config::video.vdisplay_connector;
    if (!preferred.empty()) {
      auto it = std::find_if(candidates.begin(), candidates.end(), [&](const connector_info_t &c) {
        return c.sysfs_name == preferred;
      });
      if (it != candidates.end()) {
        conn = *it;
        BOOST_LOG(info) << "[Experimental] Using configured connector: "sv << conn.sysfs_name;
      } else {
        BOOST_LOG(warning) << "[Experimental] Configured connector '"sv << preferred << "' not available, falling back to auto-select"sv;
        conn = select_best(candidates);
      }
    } else {
      conn = select_best(candidates);
    }
    BOOST_LOG(info) << "[Experimental] Selected connector: "sv << conn.sysfs_name;

    active_state = active_state_t {conn, false, false};

    if (width <= 0 || height <= 0 || fps_millihertz <= 0) {
      BOOST_LOG(error) << "[Experimental] Missing resolution/refresh parameters for virtual display"sv;
      active_state.reset();
      return status_e::EDID_WRITE_FAILED;
    }

    int refresh_hz = std::clamp((fps_millihertz + 500) / 1000, 24, 360);
    auto edid = generate_edid(width, height, refresh_hz, client_id);

    if (!write_edid(conn, edid.data(), edid.size())) {
      active_state.reset();
      return status_e::EDID_WRITE_FAILED;
    }
    active_state->edid_written = true;

    BOOST_LOG(info) << "[Experimental] Generated EDID for "sv << width << "x"sv << height << "@"sv << refresh_hz << "Hz"sv
                    << (client_id.empty() ? ""sv : " (client: "sv) << client_id << (client_id.empty() ? ""sv : ")"sv);

    if (!force_status(conn, "on")) {
      clear_edid(conn);
      active_state.reset();
      return status_e::REPROBE_FAILED;
    }
    active_state->reprobed = true;

    trigger_hotplug(conn);

    if (!wait_for_connector_active(conn, 5000)) {
      BOOST_LOG(warning) << "[Experimental] Proceeding despite compositor timeout"sv;
    }

    if (window_system == window_system_e::X11) {
      configure_xrandr(conn);
    }

    save_state(conn);
    out_display_name = conn.sysfs_name;
    return status_e::OK;
  }

  status_e
    create(std::string &out_display_name, int width, int height, int fps_millihertz, const std::string &client_id) {
    try {
      return create_impl(out_display_name, width, height, fps_millihertz, client_id);
    } catch (const std::exception &e) {
      BOOST_LOG(error) << "[Experimental] Virtual display creation threw: "sv << e.what();
      active_state.reset();
      return status_e::NOT_SUPPORTED;
    }
  }

  status_e
    remove() {
    if (!active_state) {
      return status_e::OK;
    }

    auto conn = active_state->connector;

    if (active_state->edid_written) {
      clear_edid(conn);
    }

    force_status(conn, "detect");
    trigger_hotplug(conn);
    std::this_thread::sleep_for(500ms);

    if (window_system == window_system_e::X11) {
      auto type_name = connector_type_name(conn.connector_type);
      auto output_name = type_name + "-" + std::to_string(conn.connector_index);
      auto cmd = "xrandr --output "s + output_name + " --off 2>&1";
      if (system(cmd.c_str()) != 0) {
        BOOST_LOG(debug) << "[Experimental] xrandr --off returned non-zero"sv;
      }
    }

    active_state.reset();
    clear_state();
    BOOST_LOG(info) << "[Experimental] Virtual display removed"sv;
    return status_e::OK;
  }

  bool
    is_active() {
    return active_state.has_value();
  }

  int get_kms_index() {
    if (!active_state) {
      return -1;
    }
    return find_kms_index(active_state->connector);
  }

}  // namespace linux_vdisplay
