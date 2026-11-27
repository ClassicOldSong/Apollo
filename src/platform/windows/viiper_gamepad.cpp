/**
 * @file src/platform/windows/viiper_gamepad.cpp
 * @brief Nintendo Switch 2 Pro Controller emulation through libVIIPER and USBIP.
 */
// platform includes
#include <Windows.h>
#include <SetupAPI.h>

// standard includes
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

// local includes
#include "src/config.h"
#include "src/logging.h"
#include "viiper_gamepad.h"

namespace platf::viiper {
  using namespace std::literals;
  using namespace std::chrono_literals;

  namespace {
    /**
     * @name libVIIPER C ABI
     * Mirrors of the libVIIPER v0.9 declarations used by Sunshine. The library is
     * loaded at runtime so Sunshine still starts when it is not installed.
     * @{
     */
    using handle_t = std::uintptr_t;  ///< Opaque libVIIPER server or device handle.

    /**
     * @brief libVIIPER `USBServerConfig`.
     */
    struct usb_server_config_t {
      const char *addr;  ///< USBIP listen address.
      std::uint64_t connection_timeout_ms;  ///< Connection timeout, 0 for the default.
      std::uint64_t device_handler_connect_timeout_ms;  ///< Device handler timeout, 0 for the default.
      std::uint32_t write_batch_flush_interval_ms;  ///< Write batch flush interval, 0 for the default.
    };

    /**
     * @brief libVIIPER `NS2ProDeviceState`.
     */
    struct ns2pro_state_t {
      std::uint32_t buttons;  ///< Button bitfield.
      std::uint16_t lx;  ///< Left stick X.
      std::uint16_t ly;  ///< Left stick Y.
      std::uint16_t rx;  ///< Right stick X.
      std::uint16_t ry;  ///< Right stick Y.
      std::int16_t accel_x;  ///< Raw accelerometer X.
      std::int16_t accel_y;  ///< Raw accelerometer Y.
      std::int16_t accel_z;  ///< Raw accelerometer Z.
      std::int16_t gyro_x;  ///< Raw gyroscope X.
      std::int16_t gyro_y;  ///< Raw gyroscope Y.
      std::int16_t gyro_z;  ///< Raw gyroscope Z.
    };

    /**
     * @brief libVIIPER `NS2ProMetaState`.
     */
    struct ns2pro_meta_t {
      const char *serial_number;  ///< USB serial number.
      std::uint8_t battery_level;  ///< Battery level in `0..9`.
      std::uint8_t charging;  ///< Non-zero while charging.
      std::uint8_t external_power;  ///< Non-zero when externally powered.
      std::uint16_t battery_volts;  ///< Battery voltage in mV.
    };

    /**
     * @brief libVIIPER `NS2ProOutputState`.
     */
    struct ns2pro_output_t {
      std::array<std::uint8_t, 16> left_rumble;  ///< Left actuator HD rumble bytes.
      std::array<std::uint8_t, 16> right_rumble;  ///< Right actuator HD rumble bytes.
      std::uint8_t flags;  ///< Bit 0 = rumble update, bit 1 = player LED update.
      std::uint8_t player_led_mask;  ///< Player LED mask.
    };

    using log_cb_t = void (*)(int level, const char *message);  ///< Log callback.
    using output_cb_t = void (*)(handle_t device, ns2pro_output_t output);  ///< Output callback.
    using new_server_fn = std::uint8_t (*)(usb_server_config_t *, handle_t *, log_cb_t);  ///< `NewUSBServer`.
    using close_server_fn = std::uint8_t (*)(handle_t);  ///< `CloseUSBServer`.
    using create_bus_fn = std::uint8_t (*)(handle_t, std::uint32_t *);  ///< `CreateUSBBus`.
    using create_device_fn = std::uint8_t (*)(handle_t, handle_t *, std::uint32_t, std::uint8_t, std::uint16_t, std::uint16_t, ns2pro_meta_t *);  ///< `CreateNS2ProDevice`.
    using set_state_fn = std::uint8_t (*)(handle_t, ns2pro_state_t);  ///< `SetNS2ProDeviceState`.
    using set_output_cb_fn = std::uint8_t (*)(handle_t, output_cb_t);  ///< `SetNS2ProOutputCallback`.
    using remove_device_fn = std::uint8_t (*)(handle_t);  ///< `RemoveNS2ProDevice`.

    /** @} */

    constexpr std::uint32_t NS2_B = 0x00000001;  ///< Nintendo B (south).
    constexpr std::uint32_t NS2_A = 0x00000002;  ///< Nintendo A (east).
    constexpr std::uint32_t NS2_Y = 0x00000004;  ///< Nintendo Y (west).
    constexpr std::uint32_t NS2_X = 0x00000008;  ///< Nintendo X (north).
    constexpr std::uint32_t NS2_R = 0x00000010;  ///< R shoulder.
    constexpr std::uint32_t NS2_ZR = 0x00000020;  ///< ZR trigger.
    constexpr std::uint32_t NS2_PLUS = 0x00000040;  ///< Plus.
    constexpr std::uint32_t NS2_RIGHT_STICK = 0x00000080;  ///< Right stick click.
    constexpr std::uint32_t NS2_DOWN = 0x00000100;  ///< D-pad down.
    constexpr std::uint32_t NS2_RIGHT = 0x00000200;  ///< D-pad right.
    constexpr std::uint32_t NS2_LEFT = 0x00000400;  ///< D-pad left.
    constexpr std::uint32_t NS2_UP = 0x00000800;  ///< D-pad up.
    constexpr std::uint32_t NS2_L = 0x00001000;  ///< L shoulder.
    constexpr std::uint32_t NS2_ZL = 0x00002000;  ///< ZL trigger.
    constexpr std::uint32_t NS2_MINUS = 0x00004000;  ///< Minus.
    constexpr std::uint32_t NS2_LEFT_STICK = 0x00008000;  ///< Left stick click.
    constexpr std::uint32_t NS2_HOME = 0x00010000;  ///< Home.
    constexpr std::uint32_t NS2_CAPTURE = 0x00020000;  ///< Capture.
    constexpr std::uint32_t NS2_GR = 0x00040000;  ///< Right grip button.
    constexpr std::uint32_t NS2_GL = 0x00080000;  ///< Left grip button.

    constexpr std::uint8_t TRIGGER_THRESHOLD = 32;  ///< Analog trigger value treated as a digital press.
    constexpr float STANDARD_GRAVITY = 9.80665F;  ///< m/s^2 per g.
    constexpr float ACCEL_RAW_PER_MS2 = 32767.0F / (8.0F * STANDARD_GRAVITY);  ///< SDL decodes +/-8 g.
    constexpr float GYRO_RAW_PER_RADS = 32767.0F / 34.8F;  ///< SDL decodes 34.8 rad/s full scale.
    constexpr float RUMBLE_AMP_MAX = 29000.0F;  ///< SDL's clamped HD rumble amplitude.

    std::int16_t clamp_raw(float value) {
      return static_cast<std::int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
    }

    std::uint16_t decode_amp(const std::array<std::uint8_t, 16> &actuator) {
      // Bytes 1..5 follow SDL's EncodeHDRumble layout; byte 0 is the sequence counter.
      const auto *data = &actuator[1];
      const unsigned high_amp = ((data[2] & 0x0Fu) << 12) | ((data[1] & 0xFCu) << 4);
      const unsigned low_amp = (static_cast<unsigned>(data[4]) << 8) | (data[3] & 0xC0u);
      const auto amp = static_cast<float>(std::max(high_amp, low_amp));
      return static_cast<std::uint16_t>(std::clamp(amp * 65535.0F / RUMBLE_AMP_MAX, 0.0F, 65535.0F));
    }

    /**
     * @brief Send one Switch 2 command on the bulk OUT pipe and drain its reply.
     */
    template<class write_fn_t, class read_fn_t>
    bool send_ns2_command(void *winusb, write_fn_t write_pipe, read_fn_t read_pipe, std::uint8_t cmd, std::uint8_t seq, std::uint8_t sub, std::uint8_t arg, std::string &error) {
      // Header: command, 0x91 (USB transport), sequence, subcommand, 0, payload length, 0, 0; then payload.
      std::array<UCHAR, 12> packet {cmd, 0x91, seq, sub, 0x00, 0x04, 0x00, 0x00, arg, 0x00, 0x00, 0x00};
      ULONG written = 0;
      if (!write_pipe(winusb, 0x02, packet.data(), static_cast<ULONG>(packet.size()), &written, nullptr)) {
        error = "WinUsb_WritePipe(cmd 0x" + std::format("{:02X}", cmd) + ") failed, error " + std::to_string(GetLastError());
        return false;
      }
      std::array<UCHAR, 64> reply {};
      ULONG read = 0;
      read_pipe(winusb, 0x82, reply.data(), static_cast<ULONG>(reply.size()), &read, nullptr);
      return true;
    }

    /**
     * @brief Enable input reports on every virtual Switch 2 Pro Controller's WinUSB interface.
     *
     * Mirrors the USB start-up a host performs on a real controller: enable buttons,
     * sticks, motion and rumble, select the Pro report, then enable reports. Runs on a
     * detached thread because Windows needs a moment to enumerate the new device.
     *
     * The Pro report (0x09) is the one whose buttons and sticks the HID descriptor
     * describes, so generic readers (DirectInput, Windows.Gaming.Input, browser
     * Gamepad API) can use it. It has no motion data; the Common report (0x05) has
     * motion but is vendor-defined, so only Switch-aware apps can read it. Steam and
     * SDL's Switch 2 driver send this start-up themselves and select 0x05, so they
     * still get motion.
     */
    void enable_ns2pro_reports() {
      using winusb_handle = void *;
      using init_fn = BOOL(WINAPI *)(HANDLE, winusb_handle *);
      using free_fn = BOOL(WINAPI *)(winusb_handle);
      using pipe_fn = BOOL(WINAPI *)(winusb_handle, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED);
      using policy_fn = BOOL(WINAPI *)(winusb_handle, UCHAR, ULONG, ULONG, PVOID);

      static HMODULE winusb_dll = LoadLibraryExW(L"winusb.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
      if (!winusb_dll) {
        BOOST_LOG(warning) << "winusb.dll not available; can't enable Switch 2 Pro input reports"sv;
        return;
      }
      auto winusb_init = reinterpret_cast<init_fn>(reinterpret_cast<void *>(GetProcAddress(winusb_dll, "WinUsb_Initialize")));
      auto winusb_free = reinterpret_cast<free_fn>(reinterpret_cast<void *>(GetProcAddress(winusb_dll, "WinUsb_Free")));
      auto write_pipe = reinterpret_cast<pipe_fn>(reinterpret_cast<void *>(GetProcAddress(winusb_dll, "WinUsb_WritePipe")));
      auto read_pipe = reinterpret_cast<pipe_fn>(reinterpret_cast<void *>(GetProcAddress(winusb_dll, "WinUsb_ReadPipe")));
      auto set_policy = reinterpret_cast<policy_fn>(reinterpret_cast<void *>(GetProcAddress(winusb_dll, "WinUsb_SetPipePolicy")));
      if (!winusb_init || !winusb_free || !write_pipe || !read_pipe || !set_policy) {
        return;
      }

      // DeviceInterfaceGUID that libVIIPER advertises in the Switch 2 Pro MS OS descriptor.
      static const GUID ns2pro_winusb_guid {0x7D8F1E5C, 0x9D89, 0x4E0D, {0xA2, 0xF0, 0x6D, 0x10, 0xF1, 0xF8, 0x9A, 0x8F}};

      // Why the last attempt failed, for the final log line.
      std::string error = "no Switch 2 Pro WinUSB interface found";

      // Up to 15 s: Windows can be slow to enumerate a new USBIP device.
      for (int attempt = 0; attempt < 60; ++attempt) {
        std::this_thread::sleep_for(250ms);

        HDEVINFO set = SetupDiGetClassDevsW(&ns2pro_winusb_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (set == INVALID_HANDLE_VALUE) {
          error = "SetupDiGetClassDevsW failed, error " + std::to_string(GetLastError());
          continue;
        }

        int enabled = 0;
        int held_by_other_app = 0;
        SP_DEVICE_INTERFACE_DATA iface {.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA)};
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &ns2pro_winusb_guid, i, &iface); ++i) {
          DWORD size = 0;
          SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &size, nullptr);
          std::vector<std::uint8_t> buffer(size);
          auto *detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
          detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
          if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, size, nullptr, nullptr)) {
            continue;
          }

          // WinUsb_Initialize requires a handle opened with FILE_FLAG_OVERLAPPED. The pipe
          // calls below pass no OVERLAPPED, so they still run synchronously.
          HANDLE file = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
          if (file == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
              // WinUSB allows one handle: another program (e.g. Steam) owns the interface and initializes it.
              ++held_by_other_app;
            } else {
              error = "CreateFileW failed, error " + std::to_string(err);
            }
            continue;
          }
          winusb_handle winusb = nullptr;
          if (winusb_init(file, &winusb)) {
            ULONG timeout_ms = 500;
            set_policy(winusb, 0x02, 0x03 /* PIPE_TRANSFER_TIMEOUT */, sizeof(timeout_ms), &timeout_ms);
            set_policy(winusb, 0x82, 0x03 /* PIPE_TRANSFER_TIMEOUT */, sizeof(timeout_ms), &timeout_ms);
            bool ok = send_ns2_command(winusb, write_pipe, read_pipe, 0x0C, 0, 0x04, 0x27, error);  // enable buttons, sticks, IMU, rumble
            ok = ok && send_ns2_command(winusb, write_pipe, read_pipe, 0x03, 1, 0x0A, 0x09, error);  // select Pro report (0x09), readable by generic HID apps
            ok = ok && send_ns2_command(winusb, write_pipe, read_pipe, 0x03, 2, 0x03, 0x01, error);  // enable USB input reports
            if (ok) {
              ++enabled;
            }
            winusb_free(winusb);
          } else {
            error = "WinUsb_Initialize failed, error " + std::to_string(GetLastError());
          }
          CloseHandle(file);
        }
        SetupDiDestroyDeviceInfoList(set);

        if (enabled > 0) {
          BOOST_LOG(info) << "Enabled input reports on "sv << enabled << " Switch 2 Pro controller(s)"sv;
          return;
        }
        if (held_by_other_app > 0) {
          BOOST_LOG(info) << "Switch 2 Pro controller is held by another app (e.g. Steam), which enables its input reports"sv;
          return;
        }
      }
      BOOST_LOG(warning) << "Couldn't enable Switch 2 Pro input reports over WinUSB ("sv << error << "); the host app must do it"sv;
    }

    void log_cb(int level, const char *message) {
      if (level >= 8) {
        BOOST_LOG(error) << "libVIIPER: "sv << message;
      } else if (level >= 4) {
        BOOST_LOG(warning) << "libVIIPER: "sv << message;
      } else if (level >= 0) {
        BOOST_LOG(info) << "libVIIPER: "sv << message;
      } else {
        BOOST_LOG(debug) << "libVIIPER: "sv << message;
      }
    }
  }  // namespace

  bool should_emulate_ns2pro(std::string_view configured_gamepad, std::uint8_t client_type) {
    return configured_gamepad == "switch"sv || (configured_gamepad == "auto"sv && client_type == LI_CTYPE_NINTENDO);
  }

  std::uint32_t ns2pro_buttons(std::uint32_t button_flags, std::uint8_t left_trigger, std::uint8_t right_trigger, bool by_label) {
    std::uint32_t result = 0;
    if (by_label) {
      // A Nintendo client reports A/B by their printed labels (Nintendo A arrives
      // as Moonlight A) but X/Y by position (Nintendo X, top, arrives as
      // Moonlight Y, top), so keep A/B and swap X/Y back to their labels.
      static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> face {{{A, NS2_A}, {B, NS2_B}, {X, NS2_Y}, {Y, NS2_X}}};
      for (const auto &[from, to] : face) {
        if (button_flags & from) {
          result |= to;
        }
      }
    } else {
      // Positional: Moonlight A (south) becomes Nintendo B (south), and so on.
      static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> face {{{A, NS2_B}, {B, NS2_A}, {X, NS2_Y}, {Y, NS2_X}}};
      for (const auto &[from, to] : face) {
        if (button_flags & from) {
          result |= to;
        }
      }
    }

    static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 15> map {{
      {DPAD_UP, NS2_UP},
      {DPAD_DOWN, NS2_DOWN},
      {DPAD_LEFT, NS2_LEFT},
      {DPAD_RIGHT, NS2_RIGHT},
      {START, NS2_PLUS},
      {BACK, NS2_MINUS},
      {HOME, NS2_HOME},
      {MISC_BUTTON, NS2_CAPTURE},
      {LEFT_STICK, NS2_LEFT_STICK},
      {RIGHT_STICK, NS2_RIGHT_STICK},
      {LEFT_BUTTON, NS2_L},
      {RIGHT_BUTTON, NS2_R},
      {PADDLE1, NS2_GR},
      {PADDLE2, NS2_GL},
      {TOUCHPAD_BUTTON, NS2_CAPTURE},
    }};

    for (const auto &[from, to] : map) {
      if (button_flags & from) {
        result |= to;
      }
    }
    if (left_trigger >= TRIGGER_THRESHOLD) {
      result |= NS2_ZL;
    }
    if (right_trigger >= TRIGGER_THRESHOLD) {
      result |= NS2_ZR;
    }
    return result;
  }

  std::uint16_t ns2pro_stick(std::int16_t value) {
    const auto raw = 2048L + std::lround(static_cast<float>(value) * 2047.0F / 32767.0F);
    return static_cast<std::uint16_t>(std::clamp(raw, 0L, 4095L));
  }

  std::array<std::int16_t, 3> ns2pro_accel(float x, float y, float z) {
    // SDL: sdl = (raw_x, raw_z, -raw_y) * scale
    return {clamp_raw(x * ACCEL_RAW_PER_MS2), clamp_raw(-z * ACCEL_RAW_PER_MS2), clamp_raw(y * ACCEL_RAW_PER_MS2)};
  }

  std::array<std::int16_t, 3> ns2pro_gyro(float x, float y, float z) {
    // SDL: sdl = (raw_x, raw_z, -raw_y) * scale, in rad/s
    constexpr float deg_to_raw = std::numbers::pi_v<float> / 180.0F * GYRO_RAW_PER_RADS;
    return {clamp_raw(x * deg_to_raw), clamp_raw(-z * deg_to_raw), clamp_raw(y * deg_to_raw)};
  }

  std::pair<std::uint16_t, std::uint16_t> ns2pro_rumble(const std::array<std::uint8_t, 16> &left, const std::array<std::uint8_t, 16> &right) {
    return {decode_amp(left), decode_amp(right)};
  }

  /**
   * @brief Per-controller state.
   */
  struct gamepad_t {
    handle_t device = 0;  ///< libVIIPER device handle.
    int client_relative_index = 0;  ///< Index used in feedback to the client.
    feedback_queue_t feedback_queue;  ///< Feedback queue for the owning client.
    bool by_label = false;  ///< Map face buttons by label (Nintendo client controller) instead of position.
    ns2pro_state_t state {};  ///< Last submitted state.
    std::pair<std::uint16_t, std::uint16_t> last_rumble {};  ///< Last rumble sent to the client.
  };

  /**
   * @brief Loaded library, server, and controllers.
   */
  struct context_t::impl {
    HMODULE library = nullptr;  ///< libVIIPER.dll.
    new_server_fn new_server = nullptr;  ///< `NewUSBServer`.
    close_server_fn close_server = nullptr;  ///< `CloseUSBServer`.
    create_bus_fn create_bus = nullptr;  ///< `CreateUSBBus`.
    create_device_fn create_device = nullptr;  ///< `CreateNS2ProDevice`.
    set_state_fn set_state = nullptr;  ///< `SetNS2ProDeviceState`.
    set_output_cb_fn set_output_cb = nullptr;  ///< `SetNS2ProOutputCallback`.
    remove_device_fn remove_device = nullptr;  ///< `RemoveNS2ProDevice`.
    handle_t server = 0;  ///< USBIP server handle.
    std::uint32_t bus = 0;  ///< USB bus ID.
    bool failed = false;  ///< Set after a failed initialization so it is not retried per controller.
    std::mutex mutex;  ///< Guards `gamepads` against output callbacks from Go threads.
    std::array<gamepad_t, MAX_GAMEPADS> gamepads {};  ///< Controllers by global index.

    static inline impl *instance = nullptr;  ///< Target of libVIIPER output callbacks.

    bool load() {
      std::wstring path(MAX_PATH, L'\0');
      const auto len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
      if (len == 0 || len >= path.size()) {
        return false;
      }
      path.resize(len);
      path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"libVIIPER.dll";

      library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
      if (!library) {
        BOOST_LOG(error) << "libVIIPER.dll was not found next to sunshine.exe; Switch controller emulation is unavailable"sv;
        return false;
      }

      auto resolve = [this](auto &fn, const char *name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(reinterpret_cast<void *>(GetProcAddress(library, name)));
        return fn != nullptr;
      };
      if (!resolve(new_server, "NewUSBServer") || !resolve(close_server, "CloseUSBServer") || !resolve(create_bus, "CreateUSBBus") || !resolve(create_device, "CreateNS2ProDevice") || !resolve(set_state, "SetNS2ProDeviceState") || !resolve(set_output_cb, "SetNS2ProOutputCallback") || !resolve(remove_device, "RemoveNS2ProDevice")) {
        BOOST_LOG(error) << "libVIIPER.dll is missing required Switch 2 Pro exports"sv;
        return false;
      }
      return true;
    }

    bool ensure_server() {
      if (server) {
        return true;
      }
      if (failed) {
        return false;
      }
      failed = true;
      if (!load()) {
        return false;
      }

      // Bind the USBIP server to loopback so the virtual devices are never exposed to the network.
      usb_server_config_t conf {.addr = "127.0.0.1:3241"};
      if (!new_server(&conf, &server, log_cb)) {
        BOOST_LOG(error) << "Couldn't start the libVIIPER USBIP server"sv;
        server = 0;
        return false;
      }
      if (!create_bus(server, &bus)) {
        BOOST_LOG(error) << "Couldn't create a libVIIPER USB bus"sv;
        close_server(server);
        server = 0;
        return false;
      }
      failed = false;
      instance = this;
      BOOST_LOG(info) << "libVIIPER USBIP server started on bus "sv << bus;
      return true;
    }

    static void output_cb(handle_t device, ns2pro_output_t output) {
      auto *self = instance;
      if (!self || !(output.flags & 0x01)) {
        return;
      }
      std::scoped_lock lock {self->mutex};
      for (auto &gamepad : self->gamepads) {
        if (gamepad.device != device || !gamepad.feedback_queue) {
          continue;
        }
        const auto rumble = ns2pro_rumble(output.left_rumble, output.right_rumble);
        if (rumble != gamepad.last_rumble) {
          gamepad.last_rumble = rumble;
          gamepad.feedback_queue->raise(gamepad_feedback_msg_t::make_rumble(gamepad.client_relative_index, rumble.first, rumble.second));
        }
        return;
      }
    }

    ~impl() {
      if (server) {
        for (auto &gamepad : gamepads) {
          if (gamepad.device) {
            set_output_cb(gamepad.device, nullptr);
            remove_device(gamepad.device);
          }
        }
        close_server(server);
      }
      if (instance == this) {
        instance = nullptr;
      }
      if (library) {
        FreeLibrary(library);
      }
    }
  };

  context_t::context_t():
      pimpl(std::make_unique<impl>()) {}

  context_t::~context_t() = default;

  int context_t::alloc_gamepad(const gamepad_id_t &id, feedback_queue_t feedback_queue, std::uint8_t client_type) {
    if (id.globalIndex < 0 || id.globalIndex >= MAX_GAMEPADS) {
      return -1;
    }

    std::scoped_lock lock {pimpl->mutex};
    if (!pimpl->ensure_server()) {
      return -1;
    }

    auto &gamepad = pimpl->gamepads[id.globalIndex];
    if (gamepad.device) {
      return -1;
    }

    const auto serial = std::format("SUNSHINE-NS2PRO-{:02}", id.globalIndex);
    ns2pro_meta_t meta {.serial_number = serial.c_str(), .battery_level = 9, .charging = 0, .external_power = 1, .battery_volts = 0};
    if (!pimpl->create_device(pimpl->server, &gamepad.device, pimpl->bus, 1, 0, 0, &meta)) {
      // libVIIPER drops a bus a few seconds after its last device is removed, so
      // the cached bus ID goes stale between sessions. Create a fresh bus and retry once.
      std::uint32_t new_bus = 0;
      if (!pimpl->create_bus(pimpl->server, &new_bus) || !pimpl->create_device(pimpl->server, &gamepad.device, new_bus, 1, 0, 0, &meta)) {
        BOOST_LOG(error) << "Couldn't create a libVIIPER Switch 2 Pro Controller; is usbip-win2 installed?"sv;
        gamepad.device = 0;
        return -1;
      }
      BOOST_LOG(info) << "libVIIPER bus "sv << pimpl->bus << " was gone; created bus "sv << new_bus;
      pimpl->bus = new_bus;
    }

    gamepad.client_relative_index = id.clientRelativeIndex;
    gamepad.feedback_queue = std::move(feedback_queue);
    gamepad.last_rumble = {};
    // Xbox/PlayStation-style client pads are mapped by position. A Nintendo pad (or one
    // the client can't identify) arrives already in Nintendo label order, so keep labels.
    gamepad.by_label = client_type != LI_CTYPE_XBOX && client_type != LI_CTYPE_PS;
    gamepad.state = {.lx = 2048, .ly = 2048, .rx = 2048, .ry = 2048};
    const auto rest = ns2pro_accel(0.0F, STANDARD_GRAVITY, 0.0F);
    gamepad.state.accel_x = rest[0];
    gamepad.state.accel_y = rest[1];
    gamepad.state.accel_z = rest[2];
    pimpl->set_state(gamepad.device, gamepad.state);
    pimpl->set_output_cb(gamepad.device, &impl::output_cb);

    // Request motion events from the client at 100 Hz
    gamepad.feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_ACCEL, 100));
    gamepad.feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_GYRO, 100));

    BOOST_LOG(info) << "Gamepad "sv << id.globalIndex << " will be Nintendo Switch 2 Pro controller (libVIIPER), client type "sv << static_cast<int>(client_type)
                    << ", face buttons by "sv << (gamepad.by_label ? "label"sv : "position"sv);

    // A Switch 2 Pro Controller stays silent until the host enables its USB input
    // reports. Steam does not always do this for the virtual device, so do it here.
    std::thread(enable_ns2pro_reports).detach();
    return 0;
  }

  int context_t::rebind_gamepad(const gamepad_id_t &id, feedback_queue_t feedback_queue) {
    if (!has_gamepad(id.globalIndex)) {
      return -1;
    }

    std::scoped_lock lock {pimpl->mutex};
    auto &gamepad = pimpl->gamepads[id.globalIndex];
    gamepad.client_relative_index = id.clientRelativeIndex;
    gamepad.feedback_queue = std::move(feedback_queue);
    gamepad.feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(id.clientRelativeIndex, LI_MOTION_TYPE_ACCEL, 100));
    gamepad.feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(id.clientRelativeIndex, LI_MOTION_TYPE_GYRO, 100));
    return 0;
  }

  bool context_t::has_gamepad(int nr) const {
    return nr >= 0 && nr < MAX_GAMEPADS && pimpl->gamepads[nr].device != 0;
  }

  void context_t::free_gamepad(int nr) {
    if (!has_gamepad(nr)) {
      return;
    }

    std::scoped_lock lock {pimpl->mutex};
    auto &gamepad = pimpl->gamepads[nr];
    pimpl->set_output_cb(gamepad.device, nullptr);
    if (!pimpl->remove_device(gamepad.device)) {
      BOOST_LOG(warning) << "Couldn't remove libVIIPER gamepad "sv << nr;
    }
    gamepad = {};
  }

  void context_t::update(int nr, const gamepad_state_t &state) {
    if (!has_gamepad(nr)) {
      return;
    }

    auto &gamepad = pimpl->gamepads[nr];
    gamepad.state.buttons = ns2pro_buttons(state.buttonFlags, state.lt, state.rt, gamepad.by_label);
    gamepad.state.lx = ns2pro_stick(state.lsX);
    gamepad.state.ly = ns2pro_stick(state.lsY);
    gamepad.state.rx = ns2pro_stick(state.rsX);
    gamepad.state.ry = ns2pro_stick(state.rsY);
    pimpl->set_state(gamepad.device, gamepad.state);
  }

  void context_t::motion(const gamepad_motion_t &motion) {
    if (!has_gamepad(motion.id.globalIndex)) {
      return;
    }

    auto &gamepad = pimpl->gamepads[motion.id.globalIndex];
    if (motion.motionType == LI_MOTION_TYPE_ACCEL) {
      const auto raw = ns2pro_accel(motion.x, motion.y, motion.z);
      gamepad.state.accel_x = raw[0];
      gamepad.state.accel_y = raw[1];
      gamepad.state.accel_z = raw[2];
    } else if (motion.motionType == LI_MOTION_TYPE_GYRO) {
      const auto raw = ns2pro_gyro(motion.x, motion.y, motion.z);
      gamepad.state.gyro_x = raw[0];
      gamepad.state.gyro_y = raw[1];
      gamepad.state.gyro_z = raw[2];
    } else {
      return;
    }
    pimpl->set_state(gamepad.device, gamepad.state);
  }
}  // namespace platf::viiper
