/**
 * @file src/process.cpp
 * @brief Definitions for the startup and shutdown of the apps started by a streaming Session.
 */
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include "process.h"

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "config.h"
#include "crypto.h"
#include "logging.h"
#include "platform/common.h"
#include "httpcommon.h"
#include "system_tray.h"
#include "utility.h"
#include "video.h"
#include "uuid.h"

#ifdef _WIN32
  // from_utf8() string conversion function
  #include "platform/windows/misc.h"
  #include "platform/windows/utils.h"

  // _SH constants for _wfsopen()
  #include <share.h>
#endif

#define DEFAULT_APP_IMAGE_PATH SUNSHINE_ASSETS_DIR "/box.png"

namespace proc {
  using namespace std::literals;
  namespace pt = boost::property_tree;

  proc_t proc;

#ifdef _WIN32
  VDISPLAY::DRIVER_STATUS vDisplayDriverStatus = VDISPLAY::DRIVER_STATUS::UNKNOWN;

  void onVDisplayWatchdogFailed() {
    vDisplayDriverStatus = VDISPLAY::DRIVER_STATUS::WATCHDOG_FAILED;
    VDISPLAY::closeVDisplayDevice();
  }

  void initVDisplayDriver() {
    vDisplayDriverStatus = VDISPLAY::openVDisplayDevice();
    if (vDisplayDriverStatus == VDISPLAY::DRIVER_STATUS::OK) {
      if (!VDISPLAY::startPingThread(onVDisplayWatchdogFailed)) {
        onVDisplayWatchdogFailed();
        return;
      }
    }
  }
#endif

  class deinit_t: public platf::deinit_t {
  public:
    ~deinit_t() {
      proc.terminate();
    }
  };

  std::unique_ptr<platf::deinit_t>
  init() {
    return std::make_unique<deinit_t>();
  }

  void
  terminate_process_group(boost::process::v1::child &proc, boost::process::v1::group &group, std::chrono::seconds exit_timeout) {
    if (group.valid() && platf::process_group_running((std::uintptr_t) group.native_handle())) {
      if (exit_timeout.count() > 0) {
        // Request processes in the group to exit gracefully
        if (platf::request_process_group_exit((std::uintptr_t) group.native_handle())) {
          // If the request was successful, wait for a little while for them to exit.
          BOOST_LOG(info) << "Successfully requested the app to exit. Waiting up to "sv << exit_timeout.count() << " seconds for it to close."sv;

          // group::wait_for() and similar functions are broken and deprecated, so we use a simple polling loop
          while (platf::process_group_running((std::uintptr_t) group.native_handle()) && (--exit_timeout).count() >= 0) {
            std::this_thread::sleep_for(1s);
          }

          if (exit_timeout.count() < 0) {
            BOOST_LOG(warning) << "App did not fully exit within the timeout. Terminating the app's remaining processes."sv;
          }
          else {
            BOOST_LOG(info) << "All app processes have successfully exited."sv;
          }
        }
        else {
          BOOST_LOG(info) << "App did not respond to a graceful termination request. Forcefully terminating the app's processes."sv;
        }
      }
      else {
        BOOST_LOG(info) << "No graceful exit timeout was specified for this app. Forcefully terminating the app's processes."sv;
      }

      // We always call terminate() even if we waited successfully for all processes above.
      // This ensures the process group state is consistent with the OS in boost.
      group.terminate();
      group.detach();
    }

    if (proc.valid()) {
      // avoid zombie process
      proc.detach();
    }
  }

  boost::filesystem::path
  find_working_directory(const std::string &cmd, boost::process::v1::environment &env) {
    // Parse the raw command string into parts to get the actual command portion
#ifdef _WIN32
    auto parts = boost::program_options::split_winmain(cmd);
#else
    auto parts = boost::program_options::split_unix(cmd);
#endif
    if (parts.empty()) {
      BOOST_LOG(error) << "Unable to parse command: "sv << cmd;
      return boost::filesystem::path();
    }

    BOOST_LOG(debug) << "Parsed target ["sv << parts.at(0) << "] from command ["sv << cmd << ']';

    // If the target is a URL, don't parse any further here
    if (parts.at(0).find("://") != std::string::npos) {
      return boost::filesystem::path();
    }

    // If the cmd path is not an absolute path, resolve it using our PATH variable
    boost::filesystem::path cmd_path(parts.at(0));
    if (!cmd_path.is_absolute()) {
      cmd_path = boost::process::v1::search_path(parts.at(0));
      if (cmd_path.empty()) {
        BOOST_LOG(error) << "Unable to find executable ["sv << parts.at(0) << "]. Is it in your PATH?"sv;
        return boost::filesystem::path();
      }
    }

    BOOST_LOG(debug) << "Resolved target ["sv << parts.at(0) << "] to path ["sv << cmd_path << ']';

    // Now that we have a complete path, we can just use parent_path()
    return cmd_path.parent_path();
  }

  int
  proc_t::execute(int app_id, const ctx_t& app, std::shared_ptr<rtsp_stream::launch_session_t> launch_session) {
    // Ensure starting from a clean slate
    terminate();

    // Save the original output name in case we modify it temporary later
    std::string output_name_orig = config::video.output_name;

    // Executed when returning from function
    auto fg = util::fail_guard([&]() {
      // Restore to user defined output name
      config::video.output_name = output_name_orig;
      terminate();
    });

    _app = app;
    _app_id = app_id;
    _launch_session = launch_session;

    uint32_t client_width = launch_session->width ? launch_session->width : 1920;
    uint32_t client_height = launch_session->height ? launch_session->height : 1080;

    uint32_t render_width = client_width;
    uint32_t render_height = client_height;

    int scale_factor = launch_session->scale_factor;
    if (_app.scale_factor != 100) {
      scale_factor = _app.scale_factor;
    }

    if (scale_factor != 100) {
      render_width *= ((float)scale_factor / 100);
      render_height *= ((float)scale_factor / 100);

      // Chop the last bit to ensure the scaled resolution is even numbered
      // Most odd resolutions won't work well
      render_width &= ~1;
      render_height &= ~1;
    }

#ifdef _WIN32
    if (config::video.headless_mode || launch_session->virtual_display || _app.virtual_display) {
      if (vDisplayDriverStatus != VDISPLAY::DRIVER_STATUS::OK) {
        // Try init driver again
        initVDisplayDriver();
      }

      if (vDisplayDriverStatus == VDISPLAY::DRIVER_STATUS::OK) {
        std::wstring prevPrimaryDisplayName = VDISPLAY::getPrimaryDisplay();

        // Try set the render adapter matching the capture adapter if user has specified one
        if (!config::video.adapter_name.empty()) {
          VDISPLAY::setRenderAdapterByName(platf::from_utf8(config::video.adapter_name));
        }

        auto device_uuid_str = _app.use_app_identity ? _app.uuid : launch_session->unique_id;
        auto device_name = _app.use_app_identity ? _app.name : launch_session->device_name;

        auto device_uuid = uuid_util::uuid_t::parse(device_uuid_str);

        memcpy(&launch_session->display_guid, &device_uuid, sizeof(GUID));

        std::wstring vdisplayName = VDISPLAY::createVirtualDisplay(
          device_uuid_str.c_str(),
          device_name.c_str(),
          render_width,
          render_height,
          launch_session->fps ? launch_session->fps : 60,
          launch_session->display_guid
        );

        BOOST_LOG(info) << "Virtual Display created at " << vdisplayName;

        std::wstring currentPrimaryDisplayName = VDISPLAY::getPrimaryDisplay();

        // Don't change display settings when no params are given
        if (launch_session->width && launch_session->height && launch_session->fps) {
          // Apply display settings
          VDISPLAY::changeDisplaySettings(vdisplayName.c_str(), render_width, render_height, launch_session->fps);
        }

        bool shouldActuallySetPrimary = false;

        // Determine if we need to set the virtual display as primary
        // Client request overrides local config
        bool shouldSetVDisplayPrimary = launch_session->virtual_display;
        if (!shouldSetVDisplayPrimary) {
          // App config overrides global config
          if (_app.virtual_display) {
            shouldSetVDisplayPrimary = _app.virtual_display_primary;
          } else {
            shouldSetVDisplayPrimary = config::video.set_vdisplay_primary;
          }
        }

        if (shouldSetVDisplayPrimary) {
          shouldActuallySetPrimary = (currentPrimaryDisplayName != vdisplayName);
        } else {
          shouldActuallySetPrimary = (currentPrimaryDisplayName != prevPrimaryDisplayName);
        }

        // Set primary display if needed
        if (shouldActuallySetPrimary) {
          auto disp = shouldSetVDisplayPrimary ? vdisplayName : prevPrimaryDisplayName;
          BOOST_LOG(info) << "Setting display " << disp << " primary";

          if (!VDISPLAY::setPrimaryDisplay(disp.c_str())) {
            BOOST_LOG(info) << "Setting display " << disp << " primary failed! Are you using Windows 11 24H2?";
          }
        }

        // Set virtual_display to true when everything went fine
        this->virtual_display = true;
        this->display_name = platf::to_utf8(vdisplayName);

        if (config::video.headless_mode) {
          // When using headless mode, we don't care which display user configured to use.
          // So we always set output_name to the newly created virtual display as a workaround for
          // empty name when probing graphics cards.
          config::video.output_name = this->display_name;
        }
      }
    }
#endif

    // Probe encoders again before streaming to ensure our chosen
    // encoder matches the active GPU (which could have changed
    // due to hotplugging, driver crash, primary monitor change,
    // or any number of other factors).
    if (video::probe_encoders()) {
      return 503;
    }

    // Add Stream-specific environment variables
    _env["SUNSHINE_APP_ID"] = _app.id;
    _env["SUNSHINE_APP_NAME"] = _app.name;
    _env["SUNSHINE_CLIENT_UID"] = launch_session->unique_id;
    _env["SUNSHINE_CLIENT_NAME"] = launch_session->device_name;
    _env["SUNSHINE_CLIENT_WIDTH"] = std::to_string(render_width);
    _env["SUNSHINE_CLIENT_HEIGHT"] = std::to_string(render_height);
    _env["SUNSHINE_CLIENT_RENDER_WIDTH"] = std::to_string(launch_session->width);
    _env["SUNSHINE_CLIENT_RENDER_HEIGHT"] = std::to_string(launch_session->height);
    _env["SUNSHINE_CLIENT_SCALE_FACTOR"] = std::to_string(scale_factor);
    _env["SUNSHINE_CLIENT_FPS"] = std::to_string(launch_session->fps);
    _env["SUNSHINE_CLIENT_HDR"] = launch_session->enable_hdr ? "true" : "false";
    _env["SUNSHINE_CLIENT_GCMAP"] = std::to_string(launch_session->gcmap);
    _env["SUNSHINE_CLIENT_HOST_AUDIO"] = launch_session->host_audio ? "true" : "false";
    _env["SUNSHINE_CLIENT_ENABLE_SOPS"] = launch_session->enable_sops ? "true" : "false";
    int channelCount = launch_session->surround_info & (65535);
    switch (channelCount) {
      case 2:
        _env["SUNSHINE_CLIENT_AUDIO_CONFIGURATION"] = "2.0";
        break;
      case 6:
        _env["SUNSHINE_CLIENT_AUDIO_CONFIGURATION"] = "5.1";
        break;
      case 8:
        _env["SUNSHINE_CLIENT_AUDIO_CONFIGURATION"] = "7.1";
        break;
    }
    _env["SUNSHINE_CLIENT_AUDIO_SURROUND_PARAMS"] = launch_session->surround_params;

    if (!_app.output.empty() && _app.output != "null"sv) {
#ifdef _WIN32
      // fopen() interprets the filename as an ANSI string on Windows, so we must convert it
      // to UTF-16 and use the wchar_t variants for proper Unicode log file path support.
      auto woutput = platf::from_utf8(_app.output);

      // Use _SH_DENYNO to allow us to open this log file again for writing even if it is
      // still open from a previous execution. This is required to handle the case of a
      // detached process executing again while the previous process is still running.
      _pipe.reset(_wfsopen(woutput.c_str(), L"a", _SH_DENYNO));
#else
      _pipe.reset(fopen(_app.output.c_str(), "a"));
#endif
    }

    std::error_code ec;
    _app_prep_begin = std::begin(_app.prep_cmds);
    _app_prep_it = _app_prep_begin;

    for (; _app_prep_it != std::end(_app.prep_cmds); ++_app_prep_it) {
      auto &cmd = *_app_prep_it;

      // Skip empty commands
      if (cmd.do_cmd.empty()) {
        continue;
      }

      boost::filesystem::path working_dir = _app.working_dir.empty() ?
                                              find_working_directory(cmd.do_cmd, _env) :
                                              boost::filesystem::path(_app.working_dir);
      BOOST_LOG(info) << "Executing Do Cmd: ["sv << cmd.do_cmd << "] elevated: " << cmd.elevated;
      auto child = platf::run_command(cmd.elevated, true, cmd.do_cmd, working_dir, _env, _pipe.get(), ec, nullptr);

      if (ec) {
        BOOST_LOG(error) << "Couldn't run ["sv << cmd.do_cmd << "]: System: "sv << ec.message();
        // We don't want any prep commands failing launch of the desktop.
        // This is to prevent the issue where users reboot their PC and need to log in with Sunshine.
        // permission_denied is typically returned when the user impersonation fails, which can happen when user is not signed in yet.
        if (!(_app.cmd.empty() && ec == std::errc::permission_denied)) {
          return -1;
        }
      }

      child.wait();
      auto ret = child.exit_code();
      if (ret != 0 && ec != std::errc::permission_denied) {
        BOOST_LOG(error) << '[' << cmd.do_cmd << "] failed with code ["sv << ret << ']';
        return -1;
      }
    }

    for (auto &cmd : _app.detached) {
      boost::filesystem::path working_dir = _app.working_dir.empty() ?
                                              find_working_directory(cmd, _env) :
                                              boost::filesystem::path(_app.working_dir);
      BOOST_LOG(info) << "Spawning ["sv << cmd << "] in ["sv << working_dir << ']';
      auto child = platf::run_command(_app.elevated, true, cmd, working_dir, _env, _pipe.get(), ec, nullptr);
      if (ec) {
        BOOST_LOG(warning) << "Couldn't spawn ["sv << cmd << "]: System: "sv << ec.message();
      }
      else {
        child.detach();
      }
    }

    if (_app.cmd.empty()) {
      BOOST_LOG(info) << "Executing [Desktop]"sv;
      placebo = true;
    }
    else {
      boost::filesystem::path working_dir = _app.working_dir.empty() ?
                                              find_working_directory(_app.cmd, _env) :
                                              boost::filesystem::path(_app.working_dir);
      BOOST_LOG(info) << "Executing: ["sv << _app.cmd << "] in ["sv << working_dir << ']';
      _process = platf::run_command(_app.elevated, true, _app.cmd, working_dir, _env, _pipe.get(), ec, &_process_group);
      if (ec) {
        BOOST_LOG(warning) << "Couldn't run ["sv << _app.cmd << "]: System: "sv << ec.message();
        return -1;
      }
    }

    _app_launch_time = std::chrono::steady_clock::now();

  #ifdef _WIN32
    auto resetHDRThread = std::thread([this, enable_hdr = launch_session->enable_hdr]{
      // Windows doesn't seem to be able to set HDR correctly when a display is just connected / changed resolution,
      // so we have tooggle HDR for the virtual display manually after a delay.
      auto retryInterval = 200ms;
      while (is_changing_settings_going_to_fail()) {
        if (retryInterval > 2s) {
          BOOST_LOG(warning) << "Restoring HDR settings failed due to retry timeout!";
          return;
        }
        std::this_thread::sleep_for(retryInterval);
        retryInterval *= 2;
      }

      // We should have got the actual streaming display by now
      std::string currentDisplay = this->display_name;
      if (!currentDisplay.empty()) {
        auto currentDisplayW = platf::from_utf8(currentDisplay).c_str();

        this->initial_display = currentDisplay;
        this->initial_hdr = VDISPLAY::getDisplayHDRByName(currentDisplayW);

        if (config::video.follow_client_hdr) {
          if (!VDISPLAY::setDisplayHDRByName(currentDisplayW, false)) {
            return;
          }

          if (enable_hdr) {
            if (VDISPLAY::setDisplayHDRByName(currentDisplayW, true)) {
              BOOST_LOG(info) << "HDR enabled for display " << currentDisplay;
            } else {
              BOOST_LOG(info) << "HDR enable failed for display " << currentDisplay;
            }
          }
        } else if (this->initial_hdr) {
          if (VDISPLAY::setDisplayHDRByName(currentDisplayW, false) && VDISPLAY::setDisplayHDRByName(currentDisplayW, true)) {
            BOOST_LOG(info) << "HDR toggled successfully for display " << currentDisplay;
          } else {
            BOOST_LOG(info) << "HDR toggle failed for display " << currentDisplay;
          }
        }
      }
    });

    resetHDRThread.detach();
  #endif

    fg.disable();

    // Restore to user defined output name
    config::video.output_name = output_name_orig;

#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1
    system_tray::update_tray_playing(_app.name);
#endif

    return 0;
  }

  int
  proc_t::running() {
    if (placebo) {
      return _app_id;
    }
    else if (_app.wait_all && _process_group && platf::process_group_running((std::uintptr_t) _process_group.native_handle())) {
      // The app is still running if any process in the group is still running
      return _app_id;
    }
    else if (_process.running()) {
      // The app is still running only if the initial process launched is still running
      return _app_id;
    }
    else if (_app.auto_detach && std::chrono::steady_clock::now() - _app_launch_time < 5s) {
      BOOST_LOG(info) << "App exited with code ["sv << _process.native_exit_code() << "] within 5 seconds of launch. Treating the app as a detached command."sv;
      BOOST_LOG(info) << "Adjust this behavior in the Applications tab or apps.json if this is not what you want."sv;
      placebo = true;

    #if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1
      if (_process.native_exit_code() != 0) {
        system_tray::update_tray_launch_error(proc::proc.get_last_run_app_name(), _process.native_exit_code());
      }
    #endif

      return _app_id;
    }

    // Perform cleanup actions now if needed
    if (_process) {
      terminate();
    }

    return 0;
  }

  void
  proc_t::terminate() {
    std::error_code ec;
    placebo = false;
    terminate_process_group(_process, _process_group, _app.exit_timeout);
    _process = boost::process::v1::child();
    _process_group = boost::process::v1::group();

    for (; _app_prep_it != _app_prep_begin; --_app_prep_it) {
      auto &cmd = *(_app_prep_it - 1);

      if (cmd.undo_cmd.empty()) {
        continue;
      }

      boost::filesystem::path working_dir = _app.working_dir.empty() ?
                                              find_working_directory(cmd.undo_cmd, _env) :
                                              boost::filesystem::path(_app.working_dir);
      BOOST_LOG(info) << "Executing Undo Cmd: ["sv << cmd.undo_cmd << ']';
      auto child = platf::run_command(cmd.elevated, true, cmd.undo_cmd, working_dir, _env, _pipe.get(), ec, nullptr);

      if (ec) {
        BOOST_LOG(warning) << "System: "sv << ec.message();
      }

      child.wait();
      auto ret = child.exit_code();

      if (ret != 0) {
        BOOST_LOG(warning) << "Return code ["sv << ret << ']';
      }
    }

    _pipe.reset();

#ifdef _WIN32
    if (config::video.follow_client_hdr && !this->initial_display.empty()) {
      if (VDISPLAY::setDisplayHDRByName(platf::from_utf8(this->initial_display).c_str(), this->initial_hdr)) {
        BOOST_LOG(info) << "HDR restored successfully for display " << this->initial_display;
      } else {
        BOOST_LOG(info) << "HDR restore failed for display " << this->initial_display;
      };
    }

    if (vDisplayDriverStatus == VDISPLAY::DRIVER_STATUS::OK && _launch_session && this->virtual_display) {
      if (VDISPLAY::removeVirtualDisplay(_launch_session->display_guid)) {
        BOOST_LOG(info) << "Virtual Display removed successfully";
      } else {
        BOOST_LOG(info) << "Virtual Display remove failed";
      }
    }
#endif

#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1
    bool has_run = _app_id > 0;

    // Only show the Stopped notification if we actually have an app to stop
    // Since terminate() is always run when a new app has started
    if (proc::proc.get_last_run_app_name().length() > 0 && has_run) {
      system_tray::update_tray_stopped(proc::proc.get_last_run_app_name());
    }
#endif

    _app_id = -1;
    display_name.clear();
    initial_hdr = false;
    initial_display.clear();
    _launch_session.reset();
    virtual_display = false;
  }

  const std::vector<ctx_t> &
  proc_t::get_apps() const {
    return _apps;
  }
  std::vector<ctx_t> &
  proc_t::get_apps() {
    return _apps;
  }

  // Gets application image from application list.
  // Returns image from assets directory if found there.
  // Returns default image if image configuration is not set.
  // Returns http content-type header compatible image type.
  std::string
  proc_t::get_app_image(int app_id) {
    auto iter = std::find_if(_apps.begin(), _apps.end(), [&app_id](const auto app) {
      return app.id == std::to_string(app_id);
    });
    auto app_image_path = iter == _apps.end() ? std::string() : iter->image_path;

    return validate_app_image_path(app_image_path);
  }

  std::string
  proc_t::get_last_run_app_name() {
    return _app.name;
  }

  boost::process::environment
  proc_t::get_env() {
    return _env;
  }

  proc_t::~proc_t() {
    // It's not safe to call terminate() here because our proc_t is a static variable
    // that may be destroyed after the Boost loggers have been destroyed. Instead,
    // we return a deinit_t to main() to handle termination when we're exiting.
    // Once we reach this point here, termination must have already happened.
    assert(!placebo);
    assert(!_process.running());
  }

  std::string_view::iterator
  find_match(std::string_view::iterator begin, std::string_view::iterator end) {
    int stack = 0;

    --begin;
    do {
      ++begin;
      switch (*begin) {
        case '(':
          ++stack;
          break;
        case ')':
          --stack;
      }
    } while (begin != end && stack != 0);

    if (begin == end) {
      throw std::out_of_range("Missing closing bracket \')\'");
    }
    return begin;
  }

  std::string
  parse_env_val(boost::process::v1::native_environment &env, const std::string_view &val_raw) {
    auto pos = std::begin(val_raw);
    auto dollar = std::find(pos, std::end(val_raw), '$');

    std::stringstream ss;

    while (dollar != std::end(val_raw)) {
      auto next = dollar + 1;
      if (next != std::end(val_raw)) {
        switch (*next) {
          case '(': {
            ss.write(pos, (dollar - pos));
            auto var_begin = next + 1;
            auto var_end = find_match(next, std::end(val_raw));
            auto var_name = std::string { var_begin, var_end };

#ifdef _WIN32
            // Windows treats environment variable names in a case-insensitive manner,
            // so we look for a case-insensitive match here. This is critical for
            // correctly appending to PATH on Windows.
            auto itr = std::find_if(env.cbegin(), env.cend(),
              [&](const auto &e) { return boost::iequals(e.get_name(), var_name); });
            if (itr != env.cend()) {
              // Use an existing case-insensitive match
              var_name = itr->get_name();
            }
#endif

            ss << env[var_name].to_string();

            pos = var_end + 1;
            next = var_end;

            break;
          }
          case '$':
            ss.write(pos, (next - pos));
            pos = next + 1;
            ++next;
            break;
        }

        dollar = std::find(next, std::end(val_raw), '$');
      }
      else {
        dollar = next;
      }
    }

    ss.write(pos, (dollar - pos));

    return ss.str();
  }

  std::string
  validate_app_image_path(std::string app_image_path) {
    if (app_image_path.empty()) {
      return DEFAULT_APP_IMAGE_PATH;
    }

    // get the image extension and convert it to lowercase
    auto image_extension = std::filesystem::path(app_image_path).extension().string();
    boost::to_lower(image_extension);

    // return the default box image if extension is not "png"
    if (image_extension != ".png") {
      return DEFAULT_APP_IMAGE_PATH;
    }

    // check if image is in assets directory
    auto full_image_path = std::filesystem::path(SUNSHINE_ASSETS_DIR) / app_image_path;
    if (std::filesystem::exists(full_image_path)) {
      return full_image_path.string();
    }
    else if (app_image_path == "./assets/steam.png") {
      // handle old default steam image definition
      return SUNSHINE_ASSETS_DIR "/steam.png";
    }

    // check if specified image exists
    std::error_code code;
    if (!std::filesystem::exists(app_image_path, code)) {
      // return default box image if image does not exist
      BOOST_LOG(warning) << "Couldn't find app image at path ["sv << app_image_path << ']';
      return DEFAULT_APP_IMAGE_PATH;
    }

    // image is a png, and not in assets directory
    // return only "content-type" http header compatible image type
    return app_image_path;
  }

  std::optional<std::string>
  calculate_sha256(const std::string &filename) {
    crypto::md_ctx_t ctx { EVP_MD_CTX_create() };
    if (!ctx) {
      return std::nullopt;
    }

    if (!EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr)) {
      return std::nullopt;
    }

    // Read file and update calculated SHA
    char buf[1024 * 16];
    std::ifstream file(filename, std::ifstream::binary);
    while (file.good()) {
      file.read(buf, sizeof(buf));
      if (!EVP_DigestUpdate(ctx.get(), buf, file.gcount())) {
        return std::nullopt;
      }
    }
    file.close();

    unsigned char result[SHA256_DIGEST_LENGTH];
    if (!EVP_DigestFinal_ex(ctx.get(), result, nullptr)) {
      return std::nullopt;
    }

    // Transform byte-array to string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto &byte : result) {
      ss << std::setw(2) << (int) byte;
    }
    return ss.str();
  }

  uint32_t
  calculate_crc32(const std::string &input) {
    boost::crc_32_type result;
    result.process_bytes(input.data(), input.length());
    return result.checksum();
  }

  std::tuple<std::string, std::string>
  calculate_app_id(const std::string &app_name, std::string app_image_path, int index) {
    // Generate id by hashing name with image data if present
    std::vector<std::string> to_hash;
    to_hash.push_back(app_name);
    auto file_path = validate_app_image_path(app_image_path);
    if (file_path != DEFAULT_APP_IMAGE_PATH) {
      auto file_hash = calculate_sha256(file_path);
      if (file_hash) {
        to_hash.push_back(file_hash.value());
      }
      else {
        // Fallback to just hashing image path
        to_hash.push_back(file_path);
      }
    }

    // Create combined strings for hash
    std::stringstream ss;
    for_each(to_hash.begin(), to_hash.end(), [&ss](const std::string &s) { ss << s; });
    auto input_no_index = ss.str();
    ss << index;
    auto input_with_index = ss.str();

    // CRC32 then truncate to signed 32-bit range due to client limitations
    auto id_no_index = std::to_string(abs((int32_t) calculate_crc32(input_no_index)));
    auto id_with_index = std::to_string(abs((int32_t) calculate_crc32(input_with_index)));

    return std::make_tuple(id_no_index, id_with_index);
  }

  void
  migrate_apps(pt::ptree* fileTree_p, pt::ptree* inputTree_p) {
    std::string new_app_uuid;

    if (inputTree_p) {
      auto input_uuid = inputTree_p->get_optional<std::string>("uuid"s);
      if (input_uuid && !input_uuid.value().empty()) {
        new_app_uuid = input_uuid.value();
      } else {
        new_app_uuid = uuid_util::uuid_t::generate().string();
        inputTree_p->erase("uuid");
        inputTree_p->put("uuid", new_app_uuid);
      }

      if (inputTree_p->get_child("prep-cmd").empty()) {
        inputTree_p->erase("prep-cmd");
      }

      if (inputTree_p->get_child("detached").empty()) {
        inputTree_p->erase("detached");
      }

      inputTree_p->erase("launching");
      inputTree_p->erase("index");
    }

    auto &apps_node = fileTree_p->get_child("apps"s);

    pt::ptree newApps;
    for (auto &kv : apps_node) {
      // Check if we have apps that have not got an uuid assigned
      auto app_uuid = kv.second.get_optional<std::string>("uuid"s);
      if (!app_uuid || app_uuid.value().empty()) {
        kv.second.erase("uuid");
        kv.second.put("uuid", uuid_util::uuid_t::generate().string());
        kv.second.erase("launching");
        newApps.push_back(std::make_pair("", std::move(kv.second)));
      } else {
        if (!new_app_uuid.empty() && app_uuid.value() == new_app_uuid) {
          newApps.push_back(std::make_pair("", *inputTree_p));
          new_app_uuid.clear();
        } else {
          newApps.push_back(std::make_pair("", std::move(kv.second)));
        }
      }
    }

    // Finally add the new app
    if (!new_app_uuid.empty()) {
      newApps.push_back(std::make_pair("", *inputTree_p));
    }

    fileTree_p->erase("apps");
    fileTree_p->push_back(std::make_pair("apps", newApps));
  }

  std::optional<proc::proc_t>
  parse(const std::string &file_name) {
    pt::ptree tree;

    try {
      pt::read_json(file_name, tree);

      auto &apps_node = tree.get_child("apps"s);
      auto &env_vars = tree.get_child("env"s);

      auto this_env = boost::this_process::environment();

      for (auto &[name, val] : env_vars) {
        this_env[name] = parse_env_val(this_env, val.get_value<std::string>());
      }

      std::set<std::string> ids;
      std::vector<proc::ctx_t> apps;
      int i = 0;
      for (auto &[_, app_node] : apps_node) {
        proc::ctx_t ctx;

        auto app_uuid = app_node.get_optional<std::string>("uuid"s);

        if (!app_uuid) {
          // We need an upgrade to the app list
          try {
            BOOST_LOG(info) << "Migrating app list...";
            migrate_apps(&tree, nullptr);
            pt::write_json(file_name, tree);
            BOOST_LOG(info) << "Migration complete.";
            return parse(file_name);
          } catch (std::exception &e) {
            BOOST_LOG(warning) << "Error happened wilie migrating the app list: "sv << e.what();
            return std::nullopt;
          }
        }

        auto prep_nodes_opt = app_node.get_child_optional("prep-cmd"s);
        auto detached_nodes_opt = app_node.get_child_optional("detached"s);
        auto exclude_global_prep = app_node.get_optional<bool>("exclude-global-prep-cmd"s);
        auto output = app_node.get_optional<std::string>("output"s);
        auto name = parse_env_val(this_env, app_node.get<std::string>("name"s));
        auto cmd = app_node.get_optional<std::string>("cmd"s);
        auto image_path = app_node.get_optional<std::string>("image-path"s);
        auto working_dir = app_node.get_optional<std::string>("working-dir"s);
        auto elevated = app_node.get_optional<bool>("elevated"s);
        auto auto_detach = app_node.get_optional<bool>("auto-detach"s);
        auto wait_all = app_node.get_optional<bool>("wait-all"s);
        auto exit_timeout = app_node.get_optional<int>("exit-timeout"s);
        auto virtual_display = app_node.get_optional<bool>("virtual-display"s);
        auto virtual_display_primary = app_node.get_optional<bool>("virtual-display-primary"s);
        auto resolution_scale_factor = app_node.get_optional<int>("scale-factor"s);
        auto use_app_identity = app_node.get_optional<bool>("use-app-identity"s);

        ctx.uuid = app_uuid.value();

        std::vector<proc::cmd_t> prep_cmds;
        if (!exclude_global_prep.value_or(false)) {
          prep_cmds.reserve(config::sunshine.prep_cmds.size());
          for (auto &prep_cmd : config::sunshine.prep_cmds) {
            auto do_cmd = parse_env_val(this_env, prep_cmd.do_cmd);
            auto undo_cmd = parse_env_val(this_env, prep_cmd.undo_cmd);

            prep_cmds.emplace_back(
              std::move(do_cmd),
              std::move(undo_cmd),
              prep_cmd.elevated
            );
          }
        }

        if (prep_nodes_opt) {
          auto &prep_nodes = *prep_nodes_opt;

          prep_cmds.reserve(prep_cmds.size() + prep_nodes.size());
          for (auto &[_, prep_node] : prep_nodes) {
            auto do_cmd = prep_node.get_optional<std::string>("do"s);
            auto undo_cmd = prep_node.get_optional<std::string>("undo"s);
            auto elevated = prep_node.get_optional<bool>("elevated");

            prep_cmds.emplace_back(
              parse_env_val(this_env, do_cmd.value_or("")),
              parse_env_val(this_env, undo_cmd.value_or("")),
              std::move(elevated.value_or(false)));
          }
        }

        std::vector<std::string> detached;
        if (detached_nodes_opt) {
          auto &detached_nodes = *detached_nodes_opt;

          detached.reserve(detached_nodes.size());
          for (auto &[_, detached_val] : detached_nodes) {
            detached.emplace_back(parse_env_val(this_env, detached_val.get_value<std::string>()));
          }
        }

        if (output) {
          ctx.output = parse_env_val(this_env, *output);
        }

        if (cmd) {
          ctx.cmd = parse_env_val(this_env, *cmd);
        }

        if (working_dir) {
          ctx.working_dir = parse_env_val(this_env, *working_dir);
#ifdef _WIN32
          // The working directory, unlike the command itself, should not be quoted
          // when it contains spaces. Unlike POSIX, Windows forbids quotes in paths,
          // so we can safely strip them all out here to avoid confusing the user.
          boost::erase_all(ctx.working_dir, "\"");
          ctx.working_dir += '\\';
#endif
        }

        if (image_path) {
          ctx.image_path = parse_env_val(this_env, *image_path);
        }

        ctx.elevated = elevated.value_or(false);
        ctx.auto_detach = auto_detach.value_or(true);
        ctx.wait_all = wait_all.value_or(true);
        ctx.exit_timeout = std::chrono::seconds { exit_timeout.value_or(5) };
        ctx.virtual_display = virtual_display.value_or(false);
        ctx.virtual_display_primary = virtual_display_primary.value_or(true);
        ctx.scale_factor = resolution_scale_factor.value_or(100);
        ctx.use_app_identity = use_app_identity.value_or(false);

        auto possible_ids = calculate_app_id(name, ctx.image_path, i++);
        if (ids.count(std::get<0>(possible_ids)) == 0) {
          // Avoid using index to generate id if possible
          ctx.id = std::get<0>(possible_ids);
        }
        else {
          // Fallback to include index on collision
          ctx.id = std::get<1>(possible_ids);
        }
        ids.insert(ctx.id);

        ctx.name = std::move(name);
        ctx.prep_cmds = std::move(prep_cmds);
        ctx.detached = std::move(detached);

        apps.emplace_back(std::move(ctx));
      }

    #ifdef _WIN32
      if (vDisplayDriverStatus == VDISPLAY::DRIVER_STATUS::OK) {
        proc::ctx_t ctx;
        // ctx.uuid = ""; // We're not using uuid for this special entry
        ctx.name = "Virtual Display";
        ctx.image_path = parse_env_val(this_env, "virtual_desktop.png");
        ctx.virtual_display = true;
        ctx.virtual_display_primary = true;
        ctx.scale_factor = 100;
        ctx.use_app_identity = false;

        ctx.elevated = false;
        ctx.auto_detach = true;
        ctx.wait_all = true;
        ctx.exit_timeout = 5s;

        auto possible_ids = calculate_app_id(ctx.name, ctx.image_path, i++);
        if (ids.count(std::get<0>(possible_ids)) == 0) {
          // Avoid using index to generate id if possible
          ctx.id = std::get<0>(possible_ids);
        }
        else {
          // Fallback to include index on collision
          ctx.id = std::get<1>(possible_ids);
        }
        ids.insert(ctx.id);

        apps.emplace_back(std::move(ctx));
      }
    #endif

      return proc::proc_t {
        std::move(this_env), std::move(apps)
      };
    }
    catch (std::exception &e) {
      BOOST_LOG(error) << e.what();
    }

    return std::nullopt;
  }

  void
  refresh(const std::string &file_name) {
    proc.terminate();
  #ifdef _WIN32
    if (vDisplayDriverStatus != VDISPLAY::DRIVER_STATUS::OK) {
      initVDisplayDriver();
    }
  #endif

    auto proc_opt = proc::parse(file_name);

    if (proc_opt) {
      proc = std::move(*proc_opt);
    }
  }
}  // namespace proc
