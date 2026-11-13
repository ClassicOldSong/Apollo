/**
 * @file src/platform/macos/touchscreen.mm
 * @brief Real macOS touch for a streamed display: a virtual HID touchscreen (in apollo-touchd) tied to it.
 */
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "src/logging.h"
#include "src/platform/macos/touch_protocol.h"
#include "src/platform/macos/touchscreen.h"
#include "src/platform/macos/virtual_display.h"

extern char **environ;

using namespace std::literals;

namespace platf::touchscreen {
  namespace tp = touch_protocol;

  namespace {
    // LI_TOUCH_EVENT_* (moonlight-common-c Limelight.h)
    constexpr uint8_t ev_down = 0x01, ev_up = 0x02, ev_move = 0x03, ev_cancel = 0x04, ev_cancel_all = 0x07;

    struct contact_state_t {
      uint8_t id;
      tp::contact_t last;
    };

    std::mutex lock;
    pid_t helper_pid = 0;
    int sock = -1;
    uint32_t device_display = 0;  // display the touchscreen was made for
    std::map<uint32_t, contact_state_t> contacts;  // by client pointer id

    // Contact ids are reused like a real touchscreen's: BackBoard uses them as path indexes and drops any
    // contact whose id is 30 or more (MAX_PATH_COUNT)
    uint8_t free_id() {
      for (uint8_t id = 1;; id++) {
        if (std::none_of(contacts.begin(), contacts.end(), [id](const auto &c) { return c.second.id == id; })) {
          return id;
        }
      }
    }

    int available_state = -1;  // -1 unknown, 0 no, 1 yes
    std::chrono::steady_clock::time_point available_checked;

    std::string helper_path() {
      if (const char *env = getenv("APOLLO_TOUCHD")) {
        return env;
      }
      std::vector<std::string> candidates;
      char exe[PATH_MAX];
      uint32_t size = sizeof exe;
      if (_NSGetExecutablePath(exe, &size) == 0) {
        std::string dir = exe;
        dir = dir.substr(0, dir.rfind('/'));
        candidates.push_back(dir + "/ApolloTouch.app/Contents/MacOS/apollo-touchd");
        candidates.push_back(dir + "/../Helpers/ApolloTouch.app/Contents/MacOS/apollo-touchd");
      }
      candidates.push_back("/Applications/ApolloTouch.app/Contents/MacOS/apollo-touchd");
      if (const char *home = getenv("HOME")) {
        candidates.push_back(std::string(home) + "/Applications/ApolloTouch.app/Contents/MacOS/apollo-touchd");
      }
      for (auto &c : candidates) {
        if (access(c.c_str(), X_OK) == 0) {
          return c;
        }
      }
      return {};
    }

    void stop_helper_locked() {
      if (sock >= 0) {
        close(sock);
        sock = -1;
      }
      if (helper_pid > 0) {
        // A datagram socket gives the helper no end-of-file, so end it; the device goes with it
        kill(helper_pid, SIGTERM);
        int status;
        waitpid(helper_pid, &status, 0);
        helper_pid = 0;
      }
      device_display = 0;
      contacts.clear();
    }

    bool start_helper_locked() {
      if (sock >= 0) {
        return true;
      }
      auto path = helper_path();
      if (path.empty()) {
        BOOST_LOG(info) << "Touch: apollo-touchd not found, so touch falls back to the mouse"sv;
        return false;
      }
      int fds[2];
      if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        return false;
      }
      fcntl(fds[0], F_SETFD, FD_CLOEXEC);
      fcntl(fds[1], F_SETFD, FD_CLOEXEC);  // the dup to helper_fd is inherited, the original is not
      posix_spawn_file_actions_t actions;
      posix_spawn_file_actions_init(&actions);
      posix_spawn_file_actions_adddup2(&actions, fds[1], tp::helper_fd);
      posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
      char *argv[] = {path.data(), nullptr};
      pid_t pid = 0;
      int err = posix_spawn(&pid, path.c_str(), &actions, nullptr, argv, environ);
      posix_spawn_file_actions_destroy(&actions);
      close(fds[1]);
      if (err != 0) {
        close(fds[0]);
        BOOST_LOG(warning) << "Touch: could not start "sv << path << ": "sv << strerror(err);
        return false;
      }
      timeval tv {2, 0};
      setsockopt(fds[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
      int nosigpipe = 1;
      setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof nosigpipe);
      sock = fds[0];
      helper_pid = pid;
      return true;
    }

    bool request_locked(const void *msg, size_t len, std::string &error) {
      if (send(sock, msg, len, 0) < 0) {
        error = "helper gone";
        stop_helper_locked();
        return false;
      }
      tp::reply_msg_t reply {};
      if (recv(sock, &reply, sizeof reply, 0) <= 0) {
        error = "no reply from helper";
        stop_helper_locked();
        return false;
      }
      error = reply.error;
      return reply.ok;
    }

    bool create_locked(uint32_t display_id, std::string &error) {
      if (!start_helper_locked()) {
        error = "apollo-touchd unavailable";
        return false;
      }
      tp::create_msg_t m {};
      m.type = tp::create;
      auto uuid = display_id ? VDISPLAY::displayUUID(display_id) : "00000000-0000-0000-0000-000000000000"s;
      strlcpy(m.display_uuid, uuid.c_str(), sizeof m.display_uuid);
      CGSize mm = display_id ? CGDisplayScreenSize(display_id) : CGSizeMake(300, 200);
      m.width_mm = (uint16_t) std::clamp(mm.width, 10.0, 3000.0);
      m.height_mm = (uint16_t) std::clamp(mm.height, 10.0, 3000.0);
      if (!request_locked(&m, sizeof m, error)) {
        return false;
      }
      device_display = display_id;
      contacts.clear();
      BOOST_LOG(info) << "Touch: touchscreen made for display "sv << display_id << " ("sv << uuid << ')';
      return true;
    }

    uint16_t axis(float v) {
      return (uint16_t) std::lround(std::clamp(v, 0.0f, 1.0f) * tp::axis_max);
    }

    void send_frame_locked(const std::vector<tp::contact_t> &list) {
      tp::frame_msg_t f {};
      f.type = tp::frame;
      f.count = (uint8_t) std::min<size_t>(list.size(), tp::max_contacts);
      std::copy_n(list.begin(), f.count, f.contacts);
      if (send(sock, &f, sizeof f, 0) < 0) {
        stop_helper_locked();
      }
    }
  }  // namespace

  bool available() {
    std::lock_guard lg {lock};
    auto now = std::chrono::steady_clock::now();
    if (available_state == 1 || (available_state == 0 && now - available_checked < 30s)) {
      return available_state == 1;
    }
    available_checked = now;
    std::string error;
    bool ok = sock >= 0 && device_display ? true : create_locked(0, error);
    if (ok && !device_display) {
      // made only to find out; a real one is made for the streamed display at the first touch
      uint8_t msg = tp::destroy;
      request_locked(&msg, 1, error);
    }
    available_state = ok ? 1 : 0;
    if (!ok) {
      BOOST_LOG(info) << "Touch: no virtual touchscreen ("sv << error << "), touch falls back to the mouse"sv;
    }
    return ok;
  }

  void update(uint32_t display_id, uint8_t event_type, uint32_t pointer_id, float x, float y, float major, float minor) {
    std::lock_guard lg {lock};
    if (sock < 0 || device_display != display_id) {
      std::string error;
      if (!create_locked(display_id, error)) {
        BOOST_LOG(warning) << "Touch: "sv << error;
        return;
      }
    }

    std::vector<tp::contact_t> lifted;
    if (event_type == ev_cancel_all) {
      for (auto &[_, c] : contacts) {
        lifted.push_back({c.id, 0, c.last.x, c.last.y, c.last.width, c.last.height});
      }
      contacts.clear();
    } else if (event_type == ev_up || event_type == ev_cancel) {
      if (auto it = contacts.find(pointer_id); it != contacts.end()) {
        auto c = it->second.last;
        lifted.push_back({c.id, 0, c.x, c.y, c.width, c.height});
        contacts.erase(it);
      }
    } else if (event_type == ev_down || event_type == ev_move) {
      auto it = contacts.find(pointer_id);
      if (it == contacts.end()) {
        if (event_type == ev_move || contacts.size() >= tp::max_contacts) {
          return;
        }
        it = contacts.emplace(pointer_id, contact_state_t {free_id(), {}}).first;
      }
      // Contact size: at least about a fingertip, so a client that sends none still looks like a finger
      uint16_t w = std::max(axis(major), axis(0.008f)), h = std::max(axis(minor), axis(0.008f));
      it->second.last = {it->second.id, 3, axis(x), axis(y), w, h};
    } else {
      return;  // hover and button-only events have no meaning for a touchscreen
    }

    std::vector<tp::contact_t> list = lifted;
    for (auto &[_, c] : contacts) {
      list.push_back(c.last);
    }
    send_frame_locked(list);
    if (!lifted.empty() && contacts.empty()) {
      send_frame_locked({});
    }
  }

  void reset() {
    std::lock_guard lg {lock};
    if (sock >= 0 && !contacts.empty()) {
      std::vector<tp::contact_t> lifted;
      for (auto &[_, c] : contacts) {
        lifted.push_back({c.id, 0, c.last.x, c.last.y, c.last.width, c.last.height});
      }
      send_frame_locked(lifted);
      send_frame_locked({});
    }
    stop_helper_locked();
  }
}  // namespace platf::touchscreen
