/**
 * @file src/platform/macos/virtual_display.mm
 * @brief Virtual displays on macOS through CoreGraphics' CGVirtualDisplay (the API Sidecar and AirPlay use).
 */
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

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
extern "C" CGError SLSConfigureDisplayEnabled(CGDisplayConfigRef config, CGDirectDisplayID display, bool enabled);

namespace VDISPLAY {
  namespace {
    std::mutex lock;
    std::map<std::string, CGVirtualDisplay *> displays;  // by client uid
    std::vector<CGDirectDisplayID> turned_off;
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

    // Picks the mode whose backing store is width x height pixels and whose point size is closest to pixels / scale.
    CGDisplayModeRef find_mode(CGDirectDisplayID display, uint32_t width, uint32_t height, uint32_t scale, double hz) {
      NSDictionary *opts = @{(__bridge NSString *) kCGDisplayShowDuplicateLowResolutionModes: @YES};
      NSArray *modes = CFBridgingRelease(CGDisplayCopyAllDisplayModes(display, (__bridge CFDictionaryRef) opts));
      CGDisplayModeRef best = nullptr;
      double best_score = 1e18;
      for (id m in modes) {
        auto mode = (__bridge CGDisplayModeRef) m;
        if (CGDisplayModeGetPixelWidth(mode) != width || CGDisplayModeGetPixelHeight(mode) != height) {
          continue;
        }
        double dw = (double) CGDisplayModeGetWidth(mode) - (double) width / scale;
        double dh = (double) CGDisplayModeGetHeight(mode) - (double) height / scale;
        double score = dw * dw + dh * dh + fabs(CGDisplayModeGetRefreshRate(mode) - hz);
        if (score < best_score) {
          best_score = score;
          best = mode;
        }
      }
      return best ? CGDisplayModeRetain(best) : nullptr;
    }
  }  // namespace

  DRIVER_STATUS openVDisplayDevice() {
    return NSClassFromString(@"CGVirtualDisplay") ? DRIVER_STATUS::OK : DRIVER_STATUS::FAILED;
  }

  void closeVDisplayDevice() {
    removeAllVirtualDisplays();
  }

  std::string createVirtualDisplay(const char *client_uid, const char *client_name, uint32_t width, uint32_t height, uint32_t fps, uint32_t scale) {
    if (openVDisplayDevice() != DRIVER_STATUS::OK || !width || !height) {
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

    auto descriptor = [[CGVirtualDisplayDescriptor alloc] init];
    descriptor.queue = queue;
    descriptor.name = [NSString stringWithUTF8String:(client_name && *client_name) ? client_name : "Apollo"];
    descriptor.maxPixelsWide = width;
    descriptor.maxPixelsHigh = height;
    // About 220 pixels per inch, a Retina density, so macOS treats it as a HiDPI panel.
    descriptor.sizeInMillimeters = CGSizeMake(width * 25.4 / 220.0, height * 25.4 / 220.0);
    descriptor.vendorID = 0x1209;
    descriptor.productID = 0xA011;
    descriptor.serialNum = fnv1a(uid.c_str()) | 1;
    descriptor.terminationHandler = ^(id, id) {
    };

    auto display = [[CGVirtualDisplay alloc] initWithDescriptor:descriptor];
    if (!display || !display.displayID) {
      return {};
    }

    // With hiDPI set, each mode is a point size and macOS backs it with twice the pixels;
    // the plain pixel-size mode is offered too, for scale 1.
    auto settings = [[CGVirtualDisplaySettings alloc] init];
    settings.hiDPI = scale > 1 ? 1 : 0;
    NSMutableArray *modes = [NSMutableArray array];
    if (scale > 1) {
      [modes addObject:[[CGVirtualDisplayMode alloc] initWithWidth:width / 2 height:height / 2 refreshRate:hz]];
    }
    [modes addObject:[[CGVirtualDisplayMode alloc] initWithWidth:width height:height refreshRate:hz]];
    settings.modes = modes;
    if (![display applySettings:settings]) {
      return {};
    }

    CGDirectDisplayID display_id = display.displayID;
    displays[uid] = display;

    if (!wait_online(display_id, 3.0)) {
      return {};
    }

    if (auto mode = find_mode(display_id, width, height, scale, hz)) {
      CGDisplayConfigRef config;
      if (CGBeginDisplayConfiguration(&config) == kCGErrorSuccess) {
        CGConfigureDisplayWithDisplayMode(config, display_id, mode, nullptr);
        CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
      }
      CGDisplayModeRelease(mode);
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
      char exe[PATH_MAX];
      uint32_t size = sizeof exe;
      if (_NSGetExecutablePath(exe, &size) != 0) {
        return;
      }
      std::vector<std::string> args {exe, "--display-guard", std::to_string(getpid())};
      for (auto d : ids) {
        args.push_back(std::to_string(d));
      }
      std::vector<char *> argv;
      for (auto &a : args) {
        argv.push_back(a.data());
      }
      argv.push_back(nullptr);
      posix_spawn_file_actions_t actions;
      posix_spawn_file_actions_init(&actions);
      posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
      if (posix_spawn(&guard_pid, exe, &actions, nullptr, argv.data(), environ) != 0) {
        guard_pid = 0;
      }
      posix_spawn_file_actions_destroy(&actions);
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

  bool isDisplayGuardInvocation(int argc, char **argv) {
    return argc >= 3 && !strcmp(argv[1], "--display-guard");
  }

  int displayGuardMain(int argc, char **argv) {
    pid_t parent = atoi(argv[2]);
    std::vector<CGDirectDisplayID> ids;
    for (int i = 3; i < argc; i++) {
      ids.push_back((CGDirectDisplayID) strtoul(argv[i], nullptr, 10));
    }
    // SIGTERM means the parent restored the displays itself.
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
