/**
 * @file src/platform/linux/virtual_display.cpp
 * @brief Virtual display implementation for Linux using EVDI.
 *
 * This implementation provides virtual display support on Linux using
 * EVDI (Extensible Virtual Display Interface) for creating true virtual
 * displays that are separate from physical monitors.
 *
 * When EVDI is not available, it falls back to a passthrough mode that
 * uses the existing physical monitor for capture.
 */

// standard includes
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// platform includes
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// local includes
#include "misc.h"
#include "src/config.h"
#include "src/logging.h"
#include "virtual_display.h"

using namespace std::literals;
namespace fs = std::filesystem;

namespace VDISPLAY {

  // ============================================================================
  // EVDI Types and Function Pointers (loaded dynamically)
  // ============================================================================

  // EVDI structures (matching evdi_lib.h)
  struct evdi_lib_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
  };

  struct evdi_device_context;
  typedef struct evdi_device_context *evdi_handle;

  enum evdi_device_status {
    EVDI_AVAILABLE,
    EVDI_UNRECOGNIZED,
    EVDI_NOT_PRESENT
  };

  struct evdi_mode {
    int width;
    int height;
    int refresh_rate;
    int bits_per_pixel;
    unsigned int pixel_format;
  };

  struct evdi_rect {
    int x1, y1, x2, y2;
  };

  struct evdi_buffer {
    int id;
    void *buffer;
    int width;
    int height;
    int stride;
    struct evdi_rect *rects;
    int rect_count;
  };

  struct evdi_cursor_set {
    int32_t hot_x;
    int32_t hot_y;
    uint32_t width;
    uint32_t height;
    uint8_t enabled;
    uint32_t buffer_length;
    uint32_t *buffer;
    uint32_t pixel_format;
    uint32_t stride;
  };

  struct evdi_cursor_move {
    int32_t x;
    int32_t y;
  };

  struct evdi_ddcci_data {
    uint16_t address;
    uint16_t flags;
    uint32_t buffer_length;
    uint8_t *buffer;
  };

  struct evdi_event_context {
    void (*dpms_handler)(int dpms_mode, void *user_data);
    void (*mode_changed_handler)(struct evdi_mode mode, void *user_data);
    void (*update_ready_handler)(int buffer_to_be_updated, void *user_data);
    void (*crtc_state_handler)(int state, void *user_data);
    void (*cursor_set_handler)(struct evdi_cursor_set cursor_set, void *user_data);
    void (*cursor_move_handler)(struct evdi_cursor_move cursor_move, void *user_data);
    void (*ddcci_data_handler)(struct evdi_ddcci_data ddcci_data, void *user_data);
    void *user_data;
  };

  // EVDI function pointer types
  typedef evdi_device_status (*fn_evdi_check_device)(int device);
  typedef evdi_handle (*fn_evdi_open)(int device);
  typedef int (*fn_evdi_add_device)(void);
  typedef void (*fn_evdi_close)(evdi_handle handle);
  typedef void (*fn_evdi_connect)(evdi_handle handle, const unsigned char *edid,
                                   const unsigned int edid_length,
                                   const uint32_t sku_area_limit);
  typedef void (*fn_evdi_disconnect)(evdi_handle handle);
  typedef void (*fn_evdi_grab_pixels)(evdi_handle handle, struct evdi_rect *rects, int *num_rects);
  typedef void (*fn_evdi_register_buffer)(evdi_handle handle, struct evdi_buffer buffer);
  typedef void (*fn_evdi_unregister_buffer)(evdi_handle handle, int bufferId);
  typedef bool (*fn_evdi_request_update)(evdi_handle handle, int bufferId);
  typedef void (*fn_evdi_handle_events)(evdi_handle handle, struct evdi_event_context *evtctx);
  typedef int (*fn_evdi_get_event_ready)(evdi_handle handle);
  typedef void (*fn_evdi_get_lib_version)(struct evdi_lib_version *version);

  // EVDI function pointers (loaded at runtime)
  static struct {
    void *lib_handle;
    fn_evdi_check_device check_device;
    fn_evdi_open open;
    fn_evdi_add_device add_device;
    fn_evdi_close close;
    fn_evdi_connect connect;
    fn_evdi_disconnect disconnect;
    fn_evdi_grab_pixels grab_pixels;
    fn_evdi_register_buffer register_buffer;
    fn_evdi_unregister_buffer unregister_buffer;
    fn_evdi_request_update request_update;
    fn_evdi_handle_events handle_events;
    fn_evdi_get_event_ready get_event_ready;
    fn_evdi_get_lib_version get_lib_version;
    bool loaded;
  } evdi = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false};

  // ============================================================================
  // Standard 1920x1080 EDID (used for virtual display)
  // ============================================================================

  // EDID for a generic 1920x1080@60Hz monitor
  static const unsigned char default_edid[] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,  // Header
    0x1E, 0x6D,  // Manufacturer ID (LG Display)
    0x00, 0x00,  // Product code
    0x01, 0x01, 0x01, 0x01,  // Serial number
    0x00, 0x1D,  // Week/Year of manufacture
    0x01, 0x04,  // EDID version 1.4
    0xB5,        // Video input (digital, 8-bit color depth, DisplayPort)
    0x3C, 0x22,  // Width/height in cm (60x34 = approx 27")
    0x78,        // Gamma 2.2
    0x3A,        // Features (RGB, preferred timing)
    // Chromaticity
    0xFC, 0x81, 0xA4, 0x55, 0x4D, 0x9D, 0x25, 0x12, 0x50, 0x54,
    // Established timings
    0x21, 0x08, 0x00,
    // Standard timings
    0xD1, 0xC0,  // 1920x1080@60Hz
    0x81, 0x80,  // 1280x1024@60Hz
    0x81, 0xC0,  // 1280x720@60Hz
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    // Detailed timing descriptor: 1920x1080@60Hz
    0x02, 0x3A,  // Pixel clock: 148.5 MHz
    0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
    0x58, 0x2C, 0x45, 0x00,
    0x56, 0x50, 0x21, 0x00, 0x00, 0x1E,
    // Display name descriptor
    0x00, 0x00, 0x00, 0xFC, 0x00,
    'A', 'P', 'O', 'L', 'L', 'O', ' ', 'V', 'D', 'I', 'S', 'P', '\n',
    // Display range limits
    0x00, 0x00, 0x00, 0xFD, 0x00,
    0x32, 0x4B, 0x1E, 0x51, 0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    // Extension flag and checksum (calculated)
    0x00, 0x00
  };

  // ============================================================================
  // Global State
  // ============================================================================

  static std::mutex vdisplay_mutex;
  static DRIVER_STATUS driver_status = DRIVER_STATUS::UNKNOWN;
  static std::atomic<bool> watchdog_running {false};
  static std::thread watchdog_thread;
  static bool evdi_available = false;

  // Virtual display info structure
  struct VirtualDisplayInfo {
    std::string name;
    std::string guid_str;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    int device_index;      // EVDI device index
    evdi_handle handle;    // EVDI handle
    int drm_fd;            // DRM fd for card
    bool active;
    bool using_evdi;       // true if using EVDI, false if passthrough
  };

  static std::map<std::string, VirtualDisplayInfo> virtual_displays;

  // ============================================================================
  // EVDI Library Loading
  // ============================================================================

  static bool load_evdi_library() {
    if (evdi.loaded) {
      return true;
    }

    // Try to load libevdi.so
    const char *lib_names[] = {
      "libevdi.so.1",
      "libevdi.so",
      "/usr/lib/libevdi.so.1",
      "/usr/lib/libevdi.so",
      "/usr/local/lib/libevdi.so.1",
      "/usr/local/lib/libevdi.so"
    };

    for (const auto &lib_name : lib_names) {
      evdi.lib_handle = dlopen(lib_name, RTLD_NOW);
      if (evdi.lib_handle) {
        BOOST_LOG(info) << "[VDISPLAY] Loaded EVDI library: " << lib_name;
        break;
      }
    }

    if (!evdi.lib_handle) {
      BOOST_LOG(warning) << "[VDISPLAY] Could not load libevdi.so: " << dlerror();
      BOOST_LOG(warning) << "[VDISPLAY] Virtual display will use passthrough mode.";
      BOOST_LOG(warning) << "[VDISPLAY] Install 'evdi' package for full virtual display support.";
      return false;
    }

    // Load function pointers
    #define LOAD_EVDI_FUNC(name) \
      evdi.name = (fn_evdi_##name)dlsym(evdi.lib_handle, "evdi_" #name); \
      if (!evdi.name) { \
        BOOST_LOG(error) << "[VDISPLAY] Failed to load evdi_" #name; \
        dlclose(evdi.lib_handle); \
        evdi.lib_handle = nullptr; \
        return false; \
      }

    LOAD_EVDI_FUNC(check_device);
    LOAD_EVDI_FUNC(open);
    LOAD_EVDI_FUNC(add_device);
    LOAD_EVDI_FUNC(close);
    LOAD_EVDI_FUNC(connect);
    LOAD_EVDI_FUNC(disconnect);
    LOAD_EVDI_FUNC(grab_pixels);
    LOAD_EVDI_FUNC(register_buffer);
    LOAD_EVDI_FUNC(unregister_buffer);
    LOAD_EVDI_FUNC(request_update);
    LOAD_EVDI_FUNC(handle_events);
    LOAD_EVDI_FUNC(get_event_ready);
    LOAD_EVDI_FUNC(get_lib_version);

    #undef LOAD_EVDI_FUNC

    // Check library version
    evdi_lib_version version;
    evdi.get_lib_version(&version);
    BOOST_LOG(info) << "[VDISPLAY] EVDI library version: "
                    << version.version_major << "."
                    << version.version_minor << "."
                    << version.version_patchlevel;

    evdi.loaded = true;
    return true;
  }

  static void unload_evdi_library() {
    if (evdi.lib_handle) {
      dlclose(evdi.lib_handle);
      evdi.lib_handle = nullptr;
    }
    evdi.loaded = false;
  }

  // ============================================================================
  // EVDI Module Check
  // ============================================================================

  static bool check_evdi_module_loaded() {
    // Check if evdi kernel module is loaded
    std::ifstream modules("/proc/modules");
    std::string line;
    while (std::getline(modules, line)) {
      if (line.find("evdi") != std::string::npos) {
        BOOST_LOG(info) << "[VDISPLAY] EVDI kernel module is loaded.";
        return true;
      }
    }

    // Also check sysfs
    if (fs::exists("/sys/module/evdi")) {
      BOOST_LOG(info) << "[VDISPLAY] EVDI kernel module detected via sysfs.";
      return true;
    }

    BOOST_LOG(warning) << "[VDISPLAY] EVDI kernel module is not loaded.";
    BOOST_LOG(warning) << "[VDISPLAY] Try: sudo modprobe evdi";
    return false;
  }

  // ============================================================================
  // Utility Functions
  // ============================================================================

  static std::string generate_display_name(const uuid_util::uuid_t &guid) {
    return "VIRTUAL-" + guid.string().substr(0, 8);
  }

  static int find_available_evdi_device() {
    // Find next available EVDI device
    for (int i = 0; i < 16; i++) {
      auto status = evdi.check_device(i);
      if (status == EVDI_AVAILABLE) {
        return i;
      } else if (status == EVDI_NOT_PRESENT) {
        // Device doesn't exist yet, we can add it
        int result = evdi.add_device();
        if (result >= 0) {
          BOOST_LOG(info) << "[VDISPLAY] Added new EVDI device: " << result;
          return result;
        }
      }
    }
    return -1;
  }

  static void calculate_edid_checksum(unsigned char *edid, size_t block_size = 128) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < block_size - 1; i++) {
      checksum += edid[i];
    }
    edid[block_size - 1] = 256 - checksum;
  }

  static void create_detailed_timing_descriptor(unsigned char *dtd, uint32_t width, uint32_t height, uint32_t refresh_rate) {
    // Calculate timing parameters based on CVT (Coordinated Video Timings)
    // These are approximate values for common resolutions

    uint32_t h_blank, v_blank, h_front, h_sync, v_front, v_sync;
    uint32_t pixel_clock_khz;

    if (width == 3840 && height == 2160) {
      // 4K UHD @ 60Hz
      h_blank = 560;
      v_blank = 90;
      h_front = 176;
      h_sync = 88;
      v_front = 8;
      v_sync = 10;
      pixel_clock_khz = 533250; // 533.25 MHz
    } else if (width == 2560 && height == 1440) {
      // 1440p @ 60Hz
      h_blank = 160;
      v_blank = 44;
      h_front = 48;
      h_sync = 32;
      v_front = 3;
      v_sync = 5;
      pixel_clock_khz = 241500; // 241.5 MHz
    } else if (width == 1920 && height == 1080) {
      // 1080p @ 60Hz
      h_blank = 280;
      v_blank = 45;
      h_front = 88;
      h_sync = 44;
      v_front = 4;
      v_sync = 5;
      pixel_clock_khz = 148500; // 148.5 MHz
    } else if (width == 1280 && height == 720) {
      // 720p @ 60Hz
      h_blank = 370;
      v_blank = 30;
      h_front = 110;
      h_sync = 40;
      v_front = 5;
      v_sync = 5;
      pixel_clock_khz = 74250; // 74.25 MHz
    } else {
      // Generic calculation for other resolutions
      // Using simplified CVT formula
      double h_period = (1000000.0 / refresh_rate - 550) / (height + 3);
      double h_total = width + (width * 0.15); // ~15% horizontal blanking
      pixel_clock_khz = static_cast<uint32_t>((h_total / h_period) * 1000);
      h_blank = static_cast<uint32_t>(width * 0.15);
      v_blank = 45;
      h_front = h_blank / 4;
      h_sync = h_blank / 4;
      v_front = 3;
      v_sync = 5;
    }

    uint32_t h_active = width;
    uint32_t v_active = height;

    // Pixel clock in 10kHz units
    uint16_t pixel_clock = pixel_clock_khz / 10;

    // Detailed Timing Descriptor format (18 bytes)
    dtd[0] = pixel_clock & 0xFF;
    dtd[1] = (pixel_clock >> 8) & 0xFF;

    dtd[2] = h_active & 0xFF;
    dtd[3] = h_blank & 0xFF;
    dtd[4] = ((h_active >> 8) & 0x0F) << 4 | ((h_blank >> 8) & 0x0F);

    dtd[5] = v_active & 0xFF;
    dtd[6] = v_blank & 0xFF;
    dtd[7] = ((v_active >> 8) & 0x0F) << 4 | ((v_blank >> 8) & 0x0F);

    dtd[8] = h_front & 0xFF;
    dtd[9] = h_sync & 0xFF;
    dtd[10] = ((v_front & 0x0F) << 4) | (v_sync & 0x0F);
    dtd[11] = (((h_front >> 8) & 0x03) << 6) | (((h_sync >> 8) & 0x03) << 4) |
              (((v_front >> 4) & 0x03) << 2) | ((v_sync >> 4) & 0x03);

    // Physical size (approximate based on 27" diagonal for 4K, scaled for others)
    uint32_t h_size_mm = (width * 600) / 3840;  // 600mm for 4K width
    uint32_t v_size_mm = (height * 340) / 2160; // 340mm for 4K height
    dtd[12] = h_size_mm & 0xFF;
    dtd[13] = v_size_mm & 0xFF;
    dtd[14] = ((h_size_mm >> 8) & 0x0F) << 4 | ((v_size_mm >> 8) & 0x0F);

    dtd[15] = 0; // No border
    dtd[16] = 0; // No border
    dtd[17] = 0x1E; // Digital separate sync, positive H and V
  }

  static unsigned char *generate_edid_for_resolution(uint32_t width, uint32_t height, uint32_t refresh_rate) {
    static unsigned char edid[256]; // Support for 1 extension block
    memset(edid, 0, sizeof(edid));

    // Block 0: Base EDID
    // Header
    edid[0] = 0x00;
    edid[1] = 0xFF; edid[2] = 0xFF; edid[3] = 0xFF;
    edid[4] = 0xFF; edid[5] = 0xFF; edid[6] = 0xFF;
    edid[7] = 0x00;

    // Manufacturer ID: "APL" (Apollo)
    edid[8] = 0x06; edid[9] = 0x4C;

    // Product code
    edid[10] = 0x01; edid[11] = 0x00;

    // Serial number
    edid[12] = 0x01; edid[13] = 0x00; edid[14] = 0x00; edid[15] = 0x00;

    // Week and year of manufacture (week 1, 2024)
    edid[16] = 0x01; edid[17] = 0x22;

    // EDID version 1.4
    edid[18] = 0x01; edid[19] = 0x04;

    // Video input: Digital, 8-bit color, DisplayPort
    edid[20] = 0xB5;

    // Screen size (cm) - approximate for 27"
    edid[21] = 60; // 60 cm wide
    edid[22] = 34; // 34 cm tall

    // Gamma (2.2)
    edid[23] = 0x78;

    // Features: RGB, preferred timing in DTD1
    edid[24] = 0x3A;

    // Chromaticity coordinates (standard sRGB)
    edid[25] = 0xFC; edid[26] = 0x81; edid[27] = 0xA4; edid[28] = 0x55;
    edid[29] = 0x4D; edid[30] = 0x9D; edid[31] = 0x25; edid[32] = 0x12;
    edid[33] = 0x50; edid[34] = 0x54;

    // Established timings
    edid[35] = 0x21; edid[36] = 0x08; edid[37] = 0x00;

    // Standard timings (8 entries, 2 bytes each)
    // We'll add some common resolutions
    edid[38] = 0xD1; edid[39] = 0xC0; // 1920x1080@60
    edid[40] = 0xB3; edid[41] = 0x00; // 1680x1050@60
    edid[42] = 0xA9; edid[43] = 0xC0; // 1600x900@60
    edid[44] = 0x81; edid[45] = 0x80; // 1280x1024@60
    edid[46] = 0x81; edid[47] = 0xC0; // 1280x720@60
    edid[48] = 0x01; edid[49] = 0x01; // Unused
    edid[50] = 0x01; edid[51] = 0x01; // Unused
    edid[52] = 0x01; edid[53] = 0x01; // Unused

    // Detailed Timing Descriptor 1 (preferred timing)
    create_detailed_timing_descriptor(&edid[54], width, height, refresh_rate);

    // Descriptor 2: Display name
    edid[72] = 0x00; edid[73] = 0x00; edid[74] = 0x00; edid[75] = 0xFC; edid[76] = 0x00;
    const char *name = "APOLLO VDISP";
    for (int i = 0; i < 13 && name[i]; i++) {
      edid[77 + i] = name[i];
    }
    edid[89] = '\n';

    // Descriptor 3: Display range limits
    edid[90] = 0x00; edid[91] = 0x00; edid[92] = 0x00; edid[93] = 0xFD; edid[94] = 0x00;
    edid[95] = 0x18; // Min V rate: 24 Hz
    edid[96] = 0x78; // Max V rate: 120 Hz
    edid[97] = 0x0F; // Min H rate: 15 kHz
    edid[98] = 0xA0; // Max H rate: 160 kHz
    edid[99] = 0x78; // Max pixel clock: 1200 MHz (for 4K@120Hz support)
    edid[100] = 0x00; // GTF support
    edid[101] = 0x0A; // Newline padding
    for (int i = 102; i < 108; i++) edid[i] = 0x20; // Space padding

    // Descriptor 4: Dummy/unused
    edid[108] = 0x00; edid[109] = 0x00; edid[110] = 0x00; edid[111] = 0x10; edid[112] = 0x00;
    for (int i = 113; i < 126; i++) edid[i] = 0x20;

    // Extension flag: 1 extension block (for resolutions > 1080p)
    bool needs_extension = (width > 1920 || height > 1080);
    edid[126] = needs_extension ? 0x01 : 0x00;

    // Calculate checksum for block 0
    calculate_edid_checksum(edid, 128);

    // Block 1: CEA-861 Extension (for 4K support)
    if (needs_extension) {
      edid[128] = 0x02; // CEA extension tag
      edid[129] = 0x03; // Revision 3
      edid[130] = 0x18; // DTD offset (24 bytes for data blocks)
      edid[131] = 0x72; // Native DTDs, YCbCr support

      // Video Data Block
      edid[132] = 0x47; // Video tag (0x40) + length (7)
      edid[133] = 0x90; // VIC 16: 1080p60 (native)
      edid[134] = 0x04; // VIC 4: 720p60
      edid[135] = 0x03; // VIC 3: 480p60
      edid[136] = 0x5F; // VIC 95: 4K@60Hz (VIC 95)
      edid[137] = 0x60; // VIC 96: 4K@60Hz (VIC 96)
      edid[138] = 0x61; // VIC 97: 4K@60Hz (VIC 97)
      edid[139] = 0x65; // VIC 101: 4K@120Hz

      // HDMI Vendor Specific Data Block
      edid[140] = 0x67; // Vendor tag (0x60) + length (7)
      edid[141] = 0x03; // IEEE OUI for HDMI (0x000C03)
      edid[142] = 0x0C;
      edid[143] = 0x00;
      edid[144] = 0x10; // Source physical address
      edid[145] = 0x00;
      edid[146] = 0x00; // Supports AI, DC 48/36/30 bit
      edid[147] = 0x78; // Max TMDS clock / 5 MHz = 600 MHz

      // Detailed Timing Descriptor for 4K if needed
      if (width >= 3840) {
        create_detailed_timing_descriptor(&edid[152], 3840, 2160, 60);
      }

      // Calculate checksum for block 1
      calculate_edid_checksum(&edid[128], 128);
    }

    return edid;
  }

  // ============================================================================
  // Public API Implementation
  // ============================================================================

  DRIVER_STATUS openVDisplayDevice() {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (driver_status == DRIVER_STATUS::OK) {
      return driver_status;
    }

    BOOST_LOG(info) << "[VDISPLAY] Initializing Linux virtual display driver...";

    // Try to load EVDI library
    evdi_available = load_evdi_library();

    if (evdi_available) {
      // Check if kernel module is loaded
      if (!check_evdi_module_loaded()) {
        BOOST_LOG(warning) << "[VDISPLAY] EVDI library loaded but kernel module not available.";
        BOOST_LOG(warning) << "[VDISPLAY] Falling back to passthrough mode.";
        evdi_available = false;
      }
    }

    if (evdi_available) {
      BOOST_LOG(info) << "[VDISPLAY] EVDI available - real virtual displays supported!";
    } else {
      BOOST_LOG(warning) << "[VDISPLAY] EVDI not available - using passthrough mode.";
      BOOST_LOG(warning) << "[VDISPLAY] The stream will capture the physical display.";
    }

    driver_status = DRIVER_STATUS::OK;
    BOOST_LOG(info) << "[VDISPLAY] Linux virtual display driver initialized successfully.";

    return driver_status;
  }

  void closeVDisplayDevice() {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    BOOST_LOG(info) << "[VDISPLAY] Closing Linux virtual display driver...";

    // Stop watchdog thread
    watchdog_running = false;
    if (watchdog_thread.joinable()) {
      watchdog_thread.join();
    }

    // Clean up all virtual displays
    for (auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active) {
        if (vdinfo.using_evdi && vdinfo.handle) {
          evdi.disconnect(vdinfo.handle);
          evdi.close(vdinfo.handle);
        }
        if (vdinfo.drm_fd >= 0) {
          ::close(vdinfo.drm_fd);
        }
      }
    }
    virtual_displays.clear();

    // Unload EVDI library
    unload_evdi_library();

    driver_status = DRIVER_STATUS::UNKNOWN;
    BOOST_LOG(info) << "[VDISPLAY] Linux virtual display driver closed.";
  }

  bool startPingThread(std::function<void()> failCb) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (watchdog_running) {
      return true;
    }

    watchdog_running = true;

    watchdog_thread = std::thread([failCb = std::move(failCb)]() {
      BOOST_LOG(debug) << "[VDISPLAY] Watchdog thread started.";

      while (watchdog_running) {
        std::this_thread::sleep_for(5s);

        if (!watchdog_running) {
          break;
        }

        std::lock_guard<std::mutex> lock(vdisplay_mutex);

        for (const auto &[guid, vdinfo] : virtual_displays) {
          if (vdinfo.active && vdinfo.using_evdi && vdinfo.handle) {
            // Check EVDI device health
            int ready = evdi.get_event_ready(vdinfo.handle);
            if (ready < 0) {
              BOOST_LOG(error) << "[VDISPLAY] Virtual display " << vdinfo.name << " lost!";
              if (failCb) {
                failCb();
              }
              return;
            }
          }
        }
      }

      BOOST_LOG(debug) << "[VDISPLAY] Watchdog thread stopped.";
    });

    return true;
  }

  bool setRenderAdapterByName(const std::string &adapterName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (adapterName.empty()) {
      BOOST_LOG(debug) << "[VDISPLAY] No specific adapter requested.";
      return true;
    }

    BOOST_LOG(info) << "[VDISPLAY] Adapter hint: " << adapterName;
    // On Linux, we don't need to select specific adapters for EVDI
    return true;
  }

  std::string createVirtualDisplay(
    const char *s_client_uid,
    const char *s_client_name,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    const uuid_util::uuid_t &guid
  ) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (driver_status != DRIVER_STATUS::OK) {
      BOOST_LOG(error) << "[VDISPLAY] Driver not initialized.";
      return "";
    }

    std::string guid_str = guid.string();
    std::string display_name = generate_display_name(guid);

    // Convert fps from mHz to Hz
    uint32_t fps_hz = fps / 1000;

    BOOST_LOG(info) << "[VDISPLAY] Creating virtual display: " << display_name
                    << " (W: " << width << ", H: " << height << ", FPS: " << fps_hz << ")";
    BOOST_LOG(info) << "[VDISPLAY] Client: " << s_client_name << " (" << s_client_uid << ")";

    VirtualDisplayInfo vdinfo;
    vdinfo.name = display_name;
    vdinfo.guid_str = guid_str;
    vdinfo.width = width;
    vdinfo.height = height;
    vdinfo.fps = fps;
    vdinfo.device_index = -1;
    vdinfo.handle = nullptr;
    vdinfo.drm_fd = -1;
    vdinfo.active = true;
    vdinfo.using_evdi = false;

    if (evdi_available) {
      // Create real virtual display using EVDI
      int device = find_available_evdi_device();
      if (device >= 0) {
        evdi_handle handle = evdi.open(device);
        if (handle) {
          // Generate EDID for requested resolution
          unsigned char *edid = generate_edid_for_resolution(width, height, fps_hz);

          // Determine EDID size (128 for base, 256 with extension for 4K)
          unsigned int edid_size = (width > 1920 || height > 1080) ? 256 : 128;

          // Connect with EDID (no area limit)
          BOOST_LOG(info) << "[VDISPLAY] Connecting with " << edid_size << "-byte EDID for " << width << "x" << height;
          evdi.connect(handle, edid, edid_size, 0);

          vdinfo.device_index = device;
          vdinfo.handle = handle;
          vdinfo.using_evdi = true;

          // Find the DRM card for this EVDI device
          std::string card_path = "/dev/dri/card" + std::to_string(device);
          vdinfo.drm_fd = ::open(card_path.c_str(), O_RDWR);

          BOOST_LOG(info) << "[VDISPLAY] Created EVDI virtual display on device " << device;
        } else {
          BOOST_LOG(warning) << "[VDISPLAY] Failed to open EVDI device " << device;
        }
      } else {
        BOOST_LOG(warning) << "[VDISPLAY] No available EVDI device, using passthrough.";
      }
    }

    if (!vdinfo.using_evdi) {
      // Passthrough mode - just track the virtual display logically
      BOOST_LOG(info) << "[VDISPLAY] Using passthrough mode (no EVDI).";
      BOOST_LOG(info) << "[VDISPLAY] Stream will capture primary physical display.";
    }

    virtual_displays[guid_str] = vdinfo;

    BOOST_LOG(info) << "[VDISPLAY] Virtual display created successfully: " << display_name;
    BOOST_LOG(info) << "[VDISPLAY] Mode: " << (vdinfo.using_evdi ? "EVDI (real virtual display)" : "Passthrough");

    return display_name;
  }

  bool removeVirtualDisplay(const uuid_util::uuid_t &guid) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    std::string guid_str = guid.string();

    auto it = virtual_displays.find(guid_str);
    if (it == virtual_displays.end()) {
      BOOST_LOG(warning) << "[VDISPLAY] Virtual display not found: " << guid_str;
      return false;
    }

    auto &vdinfo = it->second;
    BOOST_LOG(info) << "[VDISPLAY] Removing virtual display: " << vdinfo.name;

    if (vdinfo.using_evdi && vdinfo.handle) {
      evdi.disconnect(vdinfo.handle);
      evdi.close(vdinfo.handle);
    }

    if (vdinfo.drm_fd >= 0) {
      ::close(vdinfo.drm_fd);
    }

    virtual_displays.erase(it);

    BOOST_LOG(info) << "[VDISPLAY] Virtual display removed successfully.";
    return true;
  }

  int changeDisplaySettings(const char *deviceName, int width, int height, int refresh_rate) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    // Convert from mHz to Hz
    int refresh_hz = refresh_rate / 1000;

    BOOST_LOG(info) << "[VDISPLAY] Changing display settings for " << deviceName
                    << " to " << width << "x" << height << "@" << refresh_hz << "Hz";

    // Find the virtual display
    for (auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == deviceName) {
        vdinfo.width = width;
        vdinfo.height = height;
        vdinfo.fps = refresh_rate;

        if (vdinfo.using_evdi && vdinfo.handle) {
          // Reconnect with new EDID for new resolution
          evdi.disconnect(vdinfo.handle);
          unsigned char *edid = generate_edid_for_resolution(width, height, refresh_hz);
          unsigned int edid_size = (width > 1920 || height > 1080) ? 256 : 128;
          BOOST_LOG(info) << "[VDISPLAY] Reconnecting with " << edid_size << "-byte EDID for " << width << "x" << height;
          evdi.connect(vdinfo.handle, edid, edid_size, 0);
        }

        BOOST_LOG(info) << "[VDISPLAY] Display settings updated successfully.";
        return 0;
      }
    }

    BOOST_LOG(debug) << "[VDISPLAY] Display not found: " << deviceName;
    return 0;
  }

  int changeDisplaySettings2(const char *deviceName, int width, int height, int refresh_rate, bool bApplyIsolated) {
    if (bApplyIsolated) {
      BOOST_LOG(debug) << "[VDISPLAY] Isolated mode is implicit with EVDI.";
    }
    return changeDisplaySettings(deviceName, width, height, refresh_rate);
  }

  std::string getPrimaryDisplay() {
    // Return first connected physical display
    try {
      for (const auto &entry : fs::directory_iterator("/dev/dri")) {
        const auto &path = entry.path();
        std::string filename = path.filename().string();
        if (filename.find("card") == 0 && filename.find("render") == std::string::npos) {
          int fd = ::open(path.c_str(), O_RDWR);
          if (fd >= 0) {
            drmModeRes *res = drmModeGetResources(fd);
            if (res) {
              for (int i = 0; i < res->count_connectors; i++) {
                drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
                if (conn && conn->connection == DRM_MODE_CONNECTED) {
                  std::string name = "HDMI-A-" + std::to_string(conn->connector_type_id);
                  drmModeFreeConnector(conn);
                  drmModeFreeResources(res);
                  ::close(fd);
                  return name;
                }
                if (conn) drmModeFreeConnector(conn);
              }
              drmModeFreeResources(res);
            }
            ::close(fd);
          }
        }
      }
    } catch (...) {}
    return "";
  }

  bool setPrimaryDisplay(const char *primaryDeviceName) {
    BOOST_LOG(debug) << "[VDISPLAY] setPrimaryDisplay is a no-op on Linux.";
    return true;
  }

  bool getDisplayHDRByName(const char *displayName) {
    BOOST_LOG(debug) << "[VDISPLAY] HDR check for: " << displayName;
    // EVDI doesn't support HDR currently
    return false;
  }

  bool setDisplayHDRByName(const char *displayName, bool enableAdvancedColor) {
    BOOST_LOG(debug) << "[VDISPLAY] HDR setting not supported on Linux/EVDI.";
    return false;
  }

  std::vector<std::string> matchDisplay(const std::string &sMatch) {
    std::vector<std::string> matches;

    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name.find(sMatch) != std::string::npos) {
        matches.push_back(vdinfo.name);
      }
    }

    return matches;
  }

  // ============================================================================
  // EVDI-specific functions for KMS integration
  // ============================================================================

  /**
   * @brief Check if a display name is an EVDI virtual display.
   */
  bool isEvdiDisplay(const std::string &displayName) {
    if (!evdi_available) {
      return false;
    }

    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == displayName && vdinfo.using_evdi) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Get the DRM card index for an EVDI display.
   * @return Card index, or -1 if not found.
   */
  int getEvdiCardIndex(const std::string &displayName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == displayName && vdinfo.using_evdi) {
        return vdinfo.device_index;
      }
    }
    return -1;
  }

}  // namespace VDISPLAY
