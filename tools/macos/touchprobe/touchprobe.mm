// touchprobe: shows what macOS delivers to an app on a touch screen.
//   touchprobe [seconds] [log file]
// Puts a full-screen window on the main display and logs, with timestamps:
//   - NSScreen.touchCapabilities of every screen (macOS 27)
//   - every NSEventTypeDirectTouch event, with each NSTouch's type (direct/indirect), phase and position
//   - mouse events (to tell real touches from touches turned into clicks)
//   - tap, pan and magnification gesture recognizers firing for direct touches
#import <AppKit/AppKit.h>

static FILE *out;

static void logf_(NSString *fmt, ...) NS_FORMAT_FUNCTION(1, 2);
static void logf_(NSString *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  NSString *s = [[NSString alloc] initWithFormat:fmt arguments:ap];
  va_end(ap);
  fprintf(out, "%.3f %s\n", [NSDate timeIntervalSinceReferenceDate], s.UTF8String);
  fflush(out);
}

static const char *phase_name(NSTouchPhase p) {
  switch (p) {
    case NSTouchPhaseBegan: return "began";
    case NSTouchPhaseMoved: return "moved";
    case NSTouchPhaseStationary: return "stationary";
    case NSTouchPhaseEnded: return "ended";
    case NSTouchPhaseCancelled: return "cancelled";
    default: return "?";
  }
}

static const char *state_name(NSGestureRecognizerState s) {
  switch (s) {
    case NSGestureRecognizerStatePossible: return "possible";
    case NSGestureRecognizerStateBegan: return "began";
    case NSGestureRecognizerStateChanged: return "changed";
    case NSGestureRecognizerStateEnded: return "ended";
    case NSGestureRecognizerStateCancelled: return "cancelled";
    case NSGestureRecognizerStateFailed: return "failed";
  }
  return "?";
}

@interface ProbeView: NSView
@end

@implementation ProbeView
- (BOOL)acceptsFirstResponder {
  return YES;
}
- (void)drawRect:(NSRect)r {
  [[NSColor colorWithCalibratedRed:0.1 green:0.12 blue:0.16 alpha:1] setFill];
  NSRectFill(r);
  NSDictionary *a = @{NSFontAttributeName: [NSFont systemFontOfSize:28], NSForegroundColorAttributeName: NSColor.whiteColor};
  [@"touchprobe: touch anywhere" drawAtPoint:NSMakePoint(30, self.bounds.size.height - 60) withAttributes:a];
}
- (void)tap:(NSClickGestureRecognizer *)g {
  NSPoint p = [g locationInView:self];
  logf_(@"GESTURE tap %s at %.0f,%.0f", state_name(g.state), p.x, p.y);
}
- (void)pan:(NSPanGestureRecognizer *)g {
  NSPoint p = [g locationInView:self];
  NSPoint t = [g translationInView:self];
  logf_(@"GESTURE pan %s at %.0f,%.0f translation %.0f,%.0f", state_name(g.state), p.x, p.y, t.x, t.y);
}
- (void)magnify:(NSMagnificationGestureRecognizer *)g {
  logf_(@"GESTURE magnify %s magnification %.3f", state_name(g.state), g.magnification);
}
@end

int main(int argc, char **argv) {
  @autoreleasepool {
    double seconds = argc > 1 ? atof(argv[1]) : 30;
    out = argc > 2 ? fopen(argv[2], "w") : stdout;
    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    for (NSScreen *s in NSScreen.screens) {
      unsigned long caps = 0;
      if (@available(macOS 27.0, *)) {
        caps = s.touchCapabilities;
      }
      logf_(@"SCREEN %@ id=%@ frame=%.0fx%.0f touchCapabilities=%lu", s.localizedName, s.deviceDescription[@"NSScreenNumber"], s.frame.size.width, s.frame.size.height, caps);
    }

    NSScreen *screen = NSScreen.mainScreen;
    NSWindow *w = [[NSWindow alloc] initWithContentRect:screen.frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO screen:screen];
    w.level = NSFloatingWindowLevel;
    ProbeView *v = [[ProbeView alloc] initWithFrame:w.contentLayoutRect];
    w.contentView = v;

    NSClickGestureRecognizer *tap = [[NSClickGestureRecognizer alloc] initWithTarget:v action:@selector(tap:)];
    NSPanGestureRecognizer *pan = [[NSPanGestureRecognizer alloc] initWithTarget:v action:@selector(pan:)];
    NSMagnificationGestureRecognizer *mag = [[NSMagnificationGestureRecognizer alloc] initWithTarget:v action:@selector(magnify:)];
    for (NSGestureRecognizer *g in @[tap, pan, mag]) {
      g.allowedTouchTypes = NSTouchTypeMaskDirect;
      [v addGestureRecognizer:g];
    }
    logf_(@"RECOGNIZERS allowedTouchTypes=%lu (direct=%lu)", (unsigned long) pan.allowedTouchTypes, (unsigned long) NSTouchTypeMaskDirect);

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskAny handler:^NSEvent *(NSEvent *e) {
      switch (e.type) {
        case NSEventTypeDirectTouch: {
          NSMutableString *s = [NSMutableString string];
          for (NSTouch *t in [e touchesMatchingPhase:NSTouchPhaseAny inView:v]) {
            NSPoint p = [t locationInView:v];
            [s appendFormat:@" [%s %s %.0f,%.0f]", t.type == NSTouchTypeDirect ? "direct" : "indirect", phase_name(t.phase), p.x, p.y];
          }
          logf_(@"EVENT directTouch%@", s);
          break;
        }
        case NSEventTypeLeftMouseDown:
        case NSEventTypeLeftMouseUp:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDown:
        case NSEventTypeMouseMoved: {
          NSPoint p = [v convertPoint:e.locationInWindow fromView:nil];
          logf_(@"EVENT mouse type=%lu subtype=%d at %.0f,%.0f", (unsigned long) e.type, (int) e.subtype, p.x, p.y);
          break;
        }
        case NSEventTypeScrollWheel:
          logf_(@"EVENT scroll dy=%.1f phase=%lu", e.scrollingDeltaY, (unsigned long) e.phase);
          break;
        case NSEventTypeGesture:
        case NSEventTypeMagnify:
        case NSEventTypeSwipe:
        case NSEventTypeRotate:
          logf_(@"EVENT gesture type=%lu", (unsigned long) e.type);
          break;
        default:
          logf_(@"EVENT type=%lu", (unsigned long) e.type);
          break;
      }
      return e;
    }];

    [w makeKeyAndOrderFront:nil];
    [app activateIgnoringOtherApps:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (seconds * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      logf_(@"DONE");
      [app terminate:nil];
    });
    [app run];
  }
  return 0;
}
