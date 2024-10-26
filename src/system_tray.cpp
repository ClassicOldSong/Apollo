/**
 * @file src/system_tray.cpp
 * @brief Definitions for the system tray icon and notification system.
 */
// macros
#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1

  #if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <accctrl.h>
    #include <aclapi.h>
    #include "platform/windows/utils.h"
    #define TRAY_ICON WEB_DIR "images/apollo.ico"
    #define TRAY_ICON_PLAYING WEB_DIR "images/apollo-playing.ico"
    #define TRAY_ICON_PAUSING WEB_DIR "images/apollo-pausing.ico"
    #define TRAY_ICON_LOCKED WEB_DIR "images/apollo-locked.ico"
  #elif defined(__linux__) || defined(linux) || defined(__linux)
    #define TRAY_ICON "apollo-tray"
    #define TRAY_ICON_PLAYING "apollo-playing"
    #define TRAY_ICON_PAUSING "apollo-pausing"
    #define TRAY_ICON_LOCKED "apollo-locked"
  #elif defined(__APPLE__) || defined(__MACH__)
    #define TRAY_ICON WEB_DIR "images/logo-apollo-16.png"
    #define TRAY_ICON_PLAYING WEB_DIR "images/apollo-playing-16.png"
    #define TRAY_ICON_PAUSING WEB_DIR "images/apollo-pausing-16.png"
    #define TRAY_ICON_LOCKED WEB_DIR "images/apollo-locked-16.png"
    #include <dispatch/dispatch.h>
  #endif

  #define TRAY_MSG_NO_APP_RUNNING "[No app is running]"

  // standard includes
  #include <csignal>
  #include <string>

  // lib includes
  #include "tray/src/tray.h"
  #include <boost/filesystem.hpp>
  #include <boost/process/v1/environment.hpp>

  // local includes
  #include "config.h"
  #include "confighttp.h"
  #include "logging.h"
  #include "platform/common.h"
  #include "process.h"
  #include "network.h"
  #include "src/entry_handler.h"
  #include "version.h"

using namespace std::literals;

// system_tray namespace
namespace system_tray {
  static std::atomic<bool> tray_initialized = false;

  void
  tray_open_ui_cb(struct tray_menu *item) {
    BOOST_LOG(info) << "Opening UI from system tray"sv;
    launch_ui();
  }

  void
  tray_force_stop_cb(struct tray_menu *item) {
    BOOST_LOG(info) << "Force stop from system tray"sv;
    proc::proc.terminate();
  }

  void
  tray_restart_cb(struct tray_menu *item) {
    BOOST_LOG(info) << "Restarting from system tray"sv;

    platf::restart();
  }

  void
  tray_quit_cb(struct tray_menu *item) {
    BOOST_LOG(info) << "Quitting from system tray"sv;

  #ifdef _WIN32
    // If we're running in a service, return a special status to
    // tell it to terminate too, otherwise it will just respawn us.
    if (GetConsoleWindow() == NULL) {
      lifetime::exit_sunshine(ERROR_SHUTDOWN_IN_PROGRESS, true);
      return;
    }
  #endif

    lifetime::exit_sunshine(0, true);
  }

  // Tray menu
  static struct tray tray = {
    .icon = TRAY_ICON,
    .tooltip = PROJECT_NAME,
    .menu =
      (struct tray_menu[]) {
        // todo - use boost/locale to translate menu strings
        { .text = "Open Apollo", .cb = tray_open_ui_cb },
        { .text = "-" },
        // { .text = "-" },
        // { .text = "Donate",
        //   .submenu =
        //     (struct tray_menu[]) {
        //       { .text = "GitHub Sponsors", .cb = tray_donate_github_cb },
        //       { .text = "MEE6", .cb = tray_donate_mee6_cb },
        //       { .text = "Patreon", .cb = tray_donate_patreon_cb },
        //       { .text = "PayPal", .cb = tray_donate_paypal_cb },
        //       { .text = nullptr } } },
        // { .text = "-" },
        { .text = TRAY_MSG_NO_APP_RUNNING, .cb = tray_force_stop_cb },
        { .text = "Restart", .cb = tray_restart_cb },
        { .text = "Quit", .cb = tray_quit_cb },
        { .text = nullptr } },
    .iconPathCount = 4,
    .allIconPaths = { TRAY_ICON, TRAY_ICON_LOCKED, TRAY_ICON_PLAYING, TRAY_ICON_PAUSING },
  };

  int
  system_tray() {
  #ifdef _WIN32
    // If we're running as SYSTEM, Explorer.exe will not have permission to open our thread handle
    // to monitor for thread termination. If Explorer fails to open our thread, our tray icon
    // will persist forever if we terminate unexpectedly. To avoid this, we will modify our thread
    // DACL to add an ACE that allows SYNCHRONIZE access to Everyone.
    {
      PACL old_dacl;
      PSECURITY_DESCRIPTOR sd;
      auto error = GetSecurityInfo(GetCurrentThread(),
        SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &old_dacl,
        nullptr,
        &sd);
      if (error != ERROR_SUCCESS) {
        BOOST_LOG(warning) << "GetSecurityInfo() failed: "sv << error;
        return 1;
      }

      auto free_sd = util::fail_guard([sd]() {
        LocalFree(sd);
      });

      SID_IDENTIFIER_AUTHORITY sid_authority = SECURITY_WORLD_SID_AUTHORITY;
      PSID world_sid;
      if (!AllocateAndInitializeSid(&sid_authority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &world_sid)) {
        error = GetLastError();
        BOOST_LOG(warning) << "AllocateAndInitializeSid() failed: "sv << error;
        return 1;
      }

      auto free_sid = util::fail_guard([world_sid]() {
        FreeSid(world_sid);
      });

      EXPLICIT_ACCESS ea {};
      ea.grfAccessPermissions = SYNCHRONIZE;
      ea.grfAccessMode = GRANT_ACCESS;
      ea.grfInheritance = NO_INHERITANCE;
      ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
      ea.Trustee.ptstrName = (LPSTR) world_sid;

      PACL new_dacl;
      error = SetEntriesInAcl(1, &ea, old_dacl, &new_dacl);
      if (error != ERROR_SUCCESS) {
        BOOST_LOG(warning) << "SetEntriesInAcl() failed: "sv << error;
        return 1;
      }

      auto free_new_dacl = util::fail_guard([new_dacl]() {
        LocalFree(new_dacl);
      });

      error = SetSecurityInfo(GetCurrentThread(),
        SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        new_dacl,
        nullptr);
      if (error != ERROR_SUCCESS) {
        BOOST_LOG(warning) << "SetSecurityInfo() failed: "sv << error;
        return 1;
      }
    }

    // Wait for the shell to be initialized before registering the tray icon.
    // This ensures the tray icon works reliably after a logoff/logon cycle.
    while (GetShellWindow() == nullptr) {
      Sleep(1000);
    }
  #endif

    if (tray_init(&tray) < 0) {
      BOOST_LOG(warning) << "Failed to create system tray"sv;
      return 1;
    }
    else {
      BOOST_LOG(info) << "System tray created"sv;
    }

    tray_initialized = true;
    while (tray_loop(1) == 0) {
      BOOST_LOG(debug) << "System tray loop"sv;
    }

    return 0;
  }

  void
  run_tray() {
    // create the system tray
  #if defined(__APPLE__) || defined(__MACH__)
    // macOS requires that UI elements be created on the main thread
    // creating tray using dispatch queue does not work, although the code doesn't actually throw any (visible) errors

    // dispatch_async(dispatch_get_main_queue(), ^{
    //   system_tray();
    // });

    BOOST_LOG(info) << "system_tray() is not yet implemented for this platform."sv;
  #else  // Windows, Linux
    // create tray in separate thread

  #ifdef _WIN32
    std::string tmp_str = "Open Apollo (" + config::nvhttp.sunshine_name + ":" + std::to_string(net::map_port(confighttp::PORT_HTTPS)) + ")";
    static const std::string title_str = utf8ToAcp(tmp_str);
  #else
    static const std::string title_str = "Open Apollo (" + config::nvhttp.sunshine_name + ":" + std::to_string(net::map_port(confighttp::PORT_HTTPS)) + ")";
  #endif
    tray.menu[0].text = title_str.c_str();

    if (config::sunshine.hide_tray_controls) {
      tray.menu[1].text = nullptr;
    }

    std::thread tray_thread(system_tray);
    tray_thread.detach();
  #endif
  }

  int
  end_tray() {
    tray_initialized = false;
    tray_exit();
    return 0;
  }

  void
  update_tray_playing(std::string app_name) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON_PLAYING;

    tray_update(&tray);
    tray.icon = TRAY_ICON_PLAYING;
    tray.notification_title = "App launched";
    char msg[256];
    static char force_close_msg[256];
    snprintf(msg, std::size(msg), "%s launched.", app_name.c_str());
    snprintf(force_close_msg, std::size(force_close_msg), "Force close [%s]", app_name.c_str());
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
    strncpy(force_close_msg, utf8ToAcp(force_close_msg).c_str(), std::size(force_close_msg) - 1);
  #endif
    tray.notification_text = msg;
    tray.notification_icon = TRAY_ICON_PLAYING;
    tray.tooltip = PROJECT_NAME;
    tray.menu[2].text = force_close_msg;
    tray_update(&tray);
  }

  void
  update_tray_pausing(std::string app_name) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON_PAUSING;
    tray_update(&tray);
    char msg[256];
    snprintf(msg, std::size(msg), "Streaming paused for %s", app_name.c_str());
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
  #endif
    tray.icon = TRAY_ICON_PAUSING;
    tray.notification_title = "Stream Paused";
    tray.notification_text = msg;
    tray.notification_icon = TRAY_ICON_PAUSING;
    tray.tooltip = PROJECT_NAME;
    tray_update(&tray);
  }

  void
  update_tray_stopped(std::string app_name) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON;
    tray_update(&tray);
    char msg[256];
    snprintf(msg, std::size(msg), "Streaming stopped for %s", app_name.c_str());
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
  #endif
    tray.icon = TRAY_ICON;
    tray.notification_icon = TRAY_ICON;
    tray.notification_title = "Application Stopped";
    tray.notification_text = msg;
    tray.tooltip = PROJECT_NAME;
    tray.menu[2].text = TRAY_MSG_NO_APP_RUNNING;
    tray_update(&tray);
  }

  void
  update_tray_launch_error(std::string app_name, int exit_code) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON;
    tray_update(&tray);
    char msg[256];
    snprintf(msg, std::size(msg), "Application %s exited too fast with code %d. Click here to terminate the stream.", app_name.c_str(), exit_code);
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
  #endif
    tray.icon = TRAY_ICON;
    tray.notification_icon = TRAY_ICON;
    tray.notification_title = "Launch Error";
    tray.notification_text = msg;
    tray.notification_cb = []() {
      BOOST_LOG(info) << "Force stop from notification"sv;
      proc::proc.terminate();
    };
    tray.tooltip = PROJECT_NAME;
    tray_update(&tray);
  }

  void
  update_tray_require_pin() {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON;
    tray_update(&tray);
    tray.icon = TRAY_ICON;
    tray.notification_title = "Incoming Pairing Request";
    tray.notification_text = "Click here to complete the pairing process";
    tray.notification_icon = TRAY_ICON_LOCKED;
    tray.tooltip = PROJECT_NAME;
    tray.notification_cb = []() {
      launch_ui_with_path("/pin#PIN");
    };
    tray_update(&tray);
  }

  void
  update_tray_paired(std::string device_name) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray_update(&tray);
    char msg[256];
    snprintf(msg, std::size(msg), "Device %s paired Succesfully. Please make sure you have access to the device.", device_name.c_str());
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
  #endif
    tray.notification_title = "Device Paired Succesfully";
    tray.notification_text = msg;
    tray.notification_icon = TRAY_ICON;
    tray.tooltip = PROJECT_NAME;
    tray_update(&tray);
  }

  void
  update_tray_client_connected(std::string client_name) {
    if (!tray_initialized) {
      return;
    }

    tray.notification_title = NULL;
    tray.notification_text = NULL;
    tray.notification_cb = NULL;
    tray.notification_icon = NULL;
    tray.icon = TRAY_ICON;
    tray_update(&tray);
    char msg[256];
    snprintf(msg, std::size(msg), "%s has connected to the session.", client_name.c_str());
  #ifdef _WIN32
    strncpy(msg, utf8ToAcp(msg).c_str(), std::size(msg) - 1);
  #endif
    tray.notification_title = "Client Connected";
    tray.notification_text = msg;
    tray.notification_icon = TRAY_ICON;
    tray.tooltip = PROJECT_NAME;
    tray_update(&tray);
  }

}  // namespace system_tray
#endif
