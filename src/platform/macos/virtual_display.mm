/**
 * @file src/platform/macos/virtual_display.mm
 * @brief Virtual displays on macOS through CoreGraphics' CGVirtualDisplay (the API Sidecar and AirPlay use).
 */
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "virtual_display.h"

// CGVirtualDisplay is not in the public SDK headers; these match CoreGraphics on macOS 14 to 27.
@interface CGVirtualDisplayDescriptor: NSObject
@property (retain, nonatomic) dispatch_queue_t queue;
@property (retain, nonatomic) NSString *name;
@property (nonatomic) unsigned int maxPixelsHigh;
@property (nonatomic) unsigned int maxPixelsWide;
@property (nonatomic) CGSize sizeInMillimeters;
@property (nonatomic) unsigned int serialNum;
@property (nonatomic) unsigned int productID;
@property (nonatomic) unsigned int vendorID;
@property (copy, nonatomic) void (^terminationHandler)(id, id);
@end

@interface CGVirtualDisplayMode: NSObject
- (instancetype)initWithWidth:(unsigned int)width height:(unsigned int)height refreshRate:(double)refreshRate;
@property (readonly, nonatomic) unsigned int width;
@property (readonly, nonatomic) unsigned int height;
@property (readonly, nonatomic) double refreshRate;
@end

@interface CGVirtualDisplaySettings: NSObject
@property (retain, nonatomic) NSArray<CGVirtualDisplayMode *> *modes;
@property (nonatomic) unsigned int hiDPI;
@end

@interface CGVirtualDisplay: NSObject
- (instancetype)initWithDescriptor:(CGVirtualDisplayDescriptor *)descriptor;
- (BOOL)applySettings:(CGVirtualDisplaySettings *)settings;
@property (readonly, nonatomic) CGDirectDisplayID displayID;
@end

extern char **environ;
// SkyLight's display on/off switch (what System Settings and BetterDisplay use); looked up at run time.
static CGError SLSConfigureDisplayEnabled(CGDisplayConfigRef config, CGDirectDisplayID display, bool enabled) {
  using fn_t = CGError (*)(CGDisplayConfigRef, CGDirectDisplayID, bool);
  static fn_t fn = (fn_t) dlsym(dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight", RTLD_LAZY), "SLSConfigureDisplayEnabled");
  return fn ? fn(config, display, enabled) : kCGErrorFailure;
}

namespace VDISPLAY {
  namespace {
    std::mutex lock;
    std::map<std::string, CGVirtualDisplay *> displays;  // by client uid
    std::vector<CGDirectDisplayID> turned_off;
    std::string last_error;
    dispatch_queue_t queue;

    uint32_t fnv1a(const char *s) {
      uint32_t h = 2166136261u;
      for (; s && *s; ++s) {
        h = (h ^ (uint8_t) *s) * 16777619u;
      }
      return h;
    }

    std::vector<CGDirectDisplayID> online_displays() {
      CGDirectDisplayID ids[32];
      uint32_t n = 0;
      if (CGGetOnlineDisplayList(32, ids, &n) != kCGErrorSuccess) {
        return {};
      }
      return {ids, ids + n};
    }

    bool wait_online(CGDirectDisplayID display, double seconds) {
      for (int i = 0; i < seconds * 50; i++) {
        for (auto d : online_displays()) {
          if (d == display && CGDisplayPixelsWide(display) > 0) {
            return true;
          }
        }
        [NSThread sleepForTimeInterval:0.02];
      }
      return false;
    }

    std::string self_path() {
      char exe[PATH_MAX];
      uint32_t size = sizeof exe;
      return _NSGetExecutablePath(exe, &size) == 0 ? exe : "";
    }

    pid_t spawn_helper(const std::vector<std::string> &helper_args) {
      std::vector<std::string> args {self_path()};
      args.insert(args.end(), helper_args.begin(), helper_args.end());
      std::vector<char *> argv;
      for (auto &a : args) {
        argv.push_back(a.data());
      }
      argv.push_back(nullptr);
      posix_spawn_file_actions_t actions;
      posix_spawn_file_actions_init(&actions);
      posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
      pid_t pid = 0;
      // Its own session, so whatever kills the host's process group (a closed terminal, tmux) spares it
      posix_spawnattr_t attr;
      posix_spawnattr_init(&attr);
      posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
      if (args[0].empty() || posix_spawn(&pid, args[0].c_str(), &actions, &attr, argv.data(), environ) != 0) {
        pid = 0;
      }
      posix_spawnattr_destroy(&attr);
      posix_spawn_file_actions_destroy(&actions);
      return pid;
    }

  }  // namespace

  std::string lastError() {
    return last_error;
  }

  DRIVER_STATUS openVDisplayDevice() {
    return NSClassFromString(@"CGVirtualDisplay") ? DRIVER_STATUS::OK : DRIVER_STATUS::FAILED;
  }

  void closeVDisplayDevice() {
    removeAllVirtualDisplays();
  }

  std::string createVirtualDisplay(const char *client_uid, const char *client_name, uint32_t width, uint32_t height, uint32_t fps, uint32_t scale) {
    if (openVDisplayDevice() != DRIVER_STATUS::OK || !width || !height) {
      last_error = "CGVirtualDisplay unavailable or no size given";
      return {};
    }
    if (scale < 1) {
      scale = 1;
    }
    double hz = fps >= 1000 ? fps / 1000.0 : (fps ? fps : 60);
    std::string uid = client_uid ? client_uid : "";

    removeVirtualDisplay(uid.c_str());

    std::lock_guard lg {lock};
    if (!queue) {
      queue = dispatch_queue_create("apollo.virtual-display", DISPATCH_QUEUE_SERIAL);
    }

    // The display's size in points. macOS 27.2 gives a virtual display its HiDPI mode only when that mode is
    // at least about 525 points high (1200x525 is HiDPI, 1200x520 falls back to 1x). For a shorter client
    // (a 2244x1008 phone is 504 points high) the display is made a little larger, like a "looks like"
    // scaled mode, and capture scales it down to the client's size.
    uint32_t pt_w = width / scale, pt_h = height / scale;
    if (scale > 1 && pt_h < 540) {
      pt_w = (uint32_t) lround(pt_w * 540.0 / pt_h);
      pt_h = 540;
    }
    uint32_t px_w = pt_w * scale, px_h = pt_h * scale;

    auto descriptor = [[CGVirtualDisplayDescriptor alloc] init];
    descriptor.queue = queue;
    descriptor.name = [NSString stringWithUTF8String:(client_name && *client_name) ? client_name : "Apollo"];
    descriptor.maxPixelsWide = px_w;
    descriptor.maxPixelsHigh = px_h;
    // A Retina density (220 pixels per inch), but never a panel under about 120 mm on its short side:
    // WindowServer leaves smaller virtual displays offline (27.2: 1440x810 at 220 ppi never came online).
    double mm_w = px_w * 25.4 / 220.0, mm_h = px_h * 25.4 / 220.0;
    double grow = std::max(1.0, 120.0 / std::min(mm_w, mm_h));
    descriptor.sizeInMillimeters = CGSizeMake(mm_w * grow, mm_h * grow);
    descriptor.vendorID = 0x1209;
    descriptor.productID = 0xA011;
    // The display's identity includes its size: macOS reapplies the mode it last used for an identity,
    // and a display whose remembered mode is not on offer never comes online.
    uint32_t serial = fnv1a((uid + "/" + std::to_string(width) + "x" + std::to_string(height) + "/" + std::to_string(scale)).c_str()) | 1;
    descriptor.terminationHandler = ^(id, id) {
    };

    // A host that crashes can leave its display behind (macOS 27.3, screen locked), and a second display
    // with the same identity is refused, so fall back to a neighbouring serial number
    CGVirtualDisplay *display = nil;
    for (uint32_t attempt = 0; attempt < 4 && !(display && display.displayID); attempt++) {
      descriptor.serialNum = serial + attempt * 2;
      display = [[CGVirtualDisplay alloc] initWithDescriptor:descriptor];
    }
    if (!display || !display.displayID) {
      last_error = "CGVirtualDisplay init failed";
      return {};
    }

    // With hiDPI set, a mode is a point size and macOS backs it with twice the pixels, and picks it by default.
    auto settings = [[CGVirtualDisplaySettings alloc] init];
    settings.hiDPI = scale > 1 ? 1 : 0;
    settings.modes = @[[[CGVirtualDisplayMode alloc] initWithWidth:pt_w height:pt_h refreshRate:hz]];
    if (![display applySettings:settings]) {
      last_error = "applySettings refused the modes";
      return {};
    }

    CGDirectDisplayID display_id = display.displayID;
    displays[uid] = display;

    if (!wait_online(display_id, 3.0)) {
      displays.erase(uid);
      last_error = "display " + std::to_string(display_id) + " did not come online";
      return {};
    }

    return std::to_string(display_id);
  }

  bool removeVirtualDisplay(const char *client_uid) {
    std::lock_guard lg {lock};
    auto it = displays.find(client_uid ? client_uid : "");
    if (it == displays.end()) {
      return false;
    }
    displays.erase(it);  // releasing the CGVirtualDisplay removes the display
    return true;
  }

  void removeAllVirtualDisplays() {
    restoreDisplays();
    std::lock_guard lg {lock};
    displays.clear();
  }

  namespace {
    pid_t guard_pid = 0;

    bool set_enabled(const std::vector<CGDirectDisplayID> &ids, bool enabled) {
      CGDisplayConfigRef config;
      if (CGBeginDisplayConfiguration(&config) != kCGErrorSuccess) {
        return false;
      }
      for (auto d : ids) {
        SLSConfigureDisplayEnabled(config, d, enabled);
      }
      // kCGConfigureForAppOnly is not undone when the process exits (tested on 27.2), so the guard does that.
      return CGCompleteDisplayConfiguration(config, kCGConfigureForSession) == kCGErrorSuccess;
    }

    // Starts a copy of this executable that turns the displays back on if this process dies.
    void start_guard(const std::vector<CGDirectDisplayID> &ids) {
      std::vector<std::string> args {"--display-guard", std::to_string(getpid())};
      for (auto d : ids) {
        args.push_back(std::to_string(d));
      }
      guard_pid = spawn_helper(args);
    }

    void stop_guard() {
      if (guard_pid > 0) {
        kill(guard_pid, SIGTERM);
        waitpid(guard_pid, nullptr, 0);
        guard_pid = 0;
      }
    }
  }  // namespace

  bool isolateDisplay(uint32_t display_id) {
    std::vector<CGDirectDisplayID> others;
    for (auto d : online_displays()) {
      if (d != display_id && CGDisplayIsActive(d) && !CGDisplayIsInMirrorSet(d)) {
        others.push_back(d);
      }
    }
    if (others.empty()) {
      return true;
    }
    std::lock_guard lg {lock};
    stop_guard();
    turned_off.insert(turned_off.end(), others.begin(), others.end());
    start_guard(turned_off);
    if (!set_enabled(others, false)) {
      return false;
    }
    return true;
  }

  void restoreDisplays() {
    std::lock_guard lg {lock};
    if (turned_off.empty()) {
      return;
    }
    set_enabled(turned_off, true);
    turned_off.clear();
    stop_guard();
  }

  bool isHelperInvocation(int argc, char **argv) {
    return argc >= 3 && !strcmp(argv[1], "--display-guard");
  }

  int helperMain(int argc, char **argv) {

    pid_t parent = atoi(argv[2]);
    std::vector<CGDirectDisplayID> ids;
    for (int i = 3; i < argc; i++) {
      ids.push_back((CGDirectDisplayID) strtoul(argv[i], nullptr, 10));
    }
    // SIGTERM means the parent restored the displays itself.
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, [](int) {
      _exit(0);
    });
    int kq = kqueue();
    struct kevent ev;
    EV_SET(&ev, parent, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
    if (kq < 0 || kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
      // Parent already gone.
      set_enabled(ids, true);
      return 0;
    }
    while (kevent(kq, nullptr, 0, &ev, 1, nullptr) <= 0) {
    }
    set_enabled(ids, true);
    return 0;
  }

  std::string displayUUID(uint32_t display_id) {
    CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(display_id);
    if (!uuid) {
      return {};
    }
    NSString *s = CFBridgingRelease(CFUUIDCreateString(kCFAllocatorDefault, uuid));
    CFRelease(uuid);
    return s.UTF8String;
  }
}  // namespace VDISPLAY
