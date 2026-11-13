// vdtool: exercise src/platform/macos/virtual_display.mm by hand.
// usage: vdtool <width> <height> [hz] [scale] [isolate 0|1] [seconds]
#import <AppKit/AppKit.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/platform/macos/virtual_display.h"

static void report(const char *when) {
  CGDirectDisplayID ids[16];
  uint32_t n = 0;
  CGGetOnlineDisplayList(16, ids, &n);
  printf("[%s] main=%u online=%u\n", when, CGMainDisplayID(), n);
  for (uint32_t i = 0; i < n; i++) {
    CGDisplayModeRef m = CGDisplayCopyDisplayMode(ids[i]);
    CGRect b = CGDisplayBounds(ids[i]);
    printf("  id=%u active=%d asleep=%d mirror=%d bounds=%.0fx%.0f@%.0f,%.0f mode=%zux%zu pixels=%zux%zu %.0fHz uuid=%s\n", ids[i], CGDisplayIsActive(ids[i]), CGDisplayIsAsleep(ids[i]), CGDisplayIsInMirrorSet(ids[i]), b.size.width, b.size.height, b.origin.x, b.origin.y, m ? CGDisplayModeGetWidth(m) : 0, m ? CGDisplayModeGetHeight(m) : 0, m ? CGDisplayModeGetPixelWidth(m) : 0, m ? CGDisplayModeGetPixelHeight(m) : 0, m ? CGDisplayModeGetRefreshRate(m) : 0, VDISPLAY::displayUUID(ids[i]).c_str());
    if (m) {
      CGDisplayModeRelease(m);
    }
    if (getenv("MODES")) {
      NSArray *all = CFBridgingRelease(CGDisplayCopyAllDisplayModes(ids[i], (__bridge CFDictionaryRef) @{(__bridge NSString *) kCGDisplayShowDuplicateLowResolutionModes: @YES}));
      for (id x in all) {
        auto mm = (__bridge CGDisplayModeRef) x;
        printf("    mode %zux%zu pixels=%zux%zu %.0fHz usable=%d\n", CGDisplayModeGetWidth(mm), CGDisplayModeGetHeight(mm), CGDisplayModeGetPixelWidth(mm), CGDisplayModeGetPixelHeight(mm), CGDisplayModeGetRefreshRate(mm), CGDisplayModeIsUsableForDesktopGUI(mm));
      }
    }
  }
  for (NSScreen *s in NSScreen.screens) {
    printf("  NSScreen %s frame=%.0fx%.0f backing=%.1f\n", s.localizedName.UTF8String, s.frame.size.width, s.frame.size.height, s.backingScaleFactor);
  }
  fflush(stdout);
}

int main(int argc, char **argv) {
  if (VDISPLAY::isHelperInvocation(argc, argv)) {
    return VDISPLAY::helperMain(argc, argv);
  }
  @autoreleasepool {
    if (argc < 3) {
      fprintf(stderr, "usage: vdtool <width> <height> [hz] [scale] [isolate 0|1] [seconds]\n");
      return 2;
    }
    uint32_t w = atoi(argv[1]), h = atoi(argv[2]);
    uint32_t hz = argc > 3 ? atoi(argv[3]) : 60;
    uint32_t scale = argc > 4 ? atoi(argv[4]) : 2;
    bool isolate = argc > 5 && atoi(argv[5]);
    double secs = argc > 6 ? atof(argv[6]) : 5;
    [NSApplication sharedApplication];
    report("before");
    std::string id = VDISPLAY::createVirtualDisplay(getenv("UID_") ? getenv("UID_") : "vdtool-client", "Apollo test", w, h, hz * 1000, scale);
    printf("created display %s %s\n", id.c_str(), id.empty() ? VDISPLAY::lastError().c_str() : "");
    if (id.empty()) {
      return 1;
    }
    if (isolate) {
      printf("isolate: %d\n", VDISPLAY::isolateDisplay(std::stoul(id)));
    }
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
    report("during");
    if (getenv("SHOT")) {
      system((std::string("/usr/sbin/screencapture -x ") + getenv("SHOT")).c_str());
    }
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:secs]];
    if (getenv("CRASH")) {
      printf("exiting without restore\n");
      fflush(stdout);
      _exit(0);
    }
    VDISPLAY::removeAllVirtualDisplays();
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.5]];
    report("after");
  }
  return 0;
}
