// apollo-touchd: owns the virtual HID touchscreen that macOS 27 routes to a display as direct touch.
//
//   apollo-touchd                      serve Apollo on fd 3 (see src/platform/macos/touch_protocol.h)
//   apollo-touchd --selftest <uuid>    make a touchscreen on that display and play a tap, a drag and a pinch
//   apollo-touchd --tap <uuid> <x> <y> <seconds>   hold one finger at x, y (0 to 1) for that long
//   apollo-touchd --attach <uuid> <s>  make a touchscreen on that display and keep it, untouched, for s seconds
//
// WindowServer (SkyLight's WSHIDDisplayEventListener and BackBoard's direct touch processor) takes any HID
// service that conforms to digitizer page 0x0D usage 4 or 1 and has a "displayUUID" property holding a
// CoreGraphics display UUID, marks that display touch capable (NSScreen.touchCapabilities) and delivers the
// service's contacts there as direct touches. Creating the device needs com.apple.developer.hid.virtual.device.
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hidsystem/IOHIDUserDevice.h>

#include <mach/mach_time.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "src/platform/macos/touch_protocol.h"

using namespace touch_protocol;

namespace {
  void put16(std::vector<uint8_t> &d, uint8_t tag, int v) {
    d.push_back(tag);
    d.push_back(v & 0xff);
    d.push_back((v >> 8) & 0xff);
  }

  // A Windows-style multi-touch touchscreen: report 1 carries max_contacts finger collections and a
  // contact count; feature report 2 gives the maximum contact count.
  NSData *descriptor(int width_mm, int height_mm) {
    std::vector<uint8_t> d {0x05, 0x0D, 0x09, 0x04, 0xA1, 0x01, 0x85, 0x01};
    for (int i = 0; i < max_contacts; i++) {
      const uint8_t head[] = {
        0x09, 0x22, 0xA1, 0x02,  // Finger, logical collection
        0x09, 0x42, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02,  // Tip Switch
        0x09, 0x32, 0x81, 0x02,  // In Range
        0x95, 0x06, 0x81, 0x03,  // padding
        0x75, 0x08, 0x95, 0x01, 0x09, 0x51, 0x25, 0x7F, 0x81, 0x02,  // Contact Identifier
        0x05, 0x01, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x55, 0x0E, 0x65, 0x11, 0x35, 0x00,  // 0..32767, units of 0.1 mm
      };
      d.insert(d.end(), std::begin(head), std::end(head));
      d.insert(d.end(), {0x09, 0x30});  // X
      put16(d, 0x46, width_mm * 10);
      d.insert(d.end(), {0x81, 0x02, 0x09, 0x31});  // Y
      put16(d, 0x46, height_mm * 10);
      d.insert(d.end(), {0x81, 0x02, 0x05, 0x0D, 0x09, 0x48});  // Width
      put16(d, 0x46, width_mm * 10);
      d.insert(d.end(), {0x81, 0x02, 0x09, 0x49});  // Height
      put16(d, 0x46, height_mm * 10);
      d.insert(d.end(), {0x81, 0x02, 0xC0});
    }
    const uint8_t tail[] = {
      0x55, 0x00, 0x65, 0x00, 0x35, 0x00, 0x45, 0x00,
      0x05, 0x0D, 0x09, 0x54, 0x15, 0x00, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02,  // Contact Count
      0x85, 0x02, 0x09, 0x55, 0x25, max_contacts, 0xB1, 0x02,  // Contact Count Maximum (feature)
      0xC0,
    };
    d.insert(d.end(), std::begin(tail), std::end(tail));
    return [NSData dataWithBytes:d.data() length:d.size()];
  }

#pragma pack(push, 1)
  struct hid_contact_t {
    uint8_t flags, id;
    uint16_t x, y, w, h;
  };

  struct hid_report_t {
    uint8_t report_id;
    hid_contact_t c[max_contacts];
    uint8_t count;
  };
#pragma pack(pop)

  IOHIDUserDeviceRef device;
  dispatch_semaphore_t device_cancelled;
  dispatch_queue_t queue;

  // An activated device has to be cancelled, and the cancel finished, before its last release
  void destroy_device() {
    if (!device) {
      return;
    }
    IOHIDUserDeviceCancel(device);
    dispatch_semaphore_wait(device_cancelled, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    CFRelease(device);
    device = nullptr;
  }

  bool create_device(const char *uuid, int width_mm, int height_mm, char *error, size_t error_len) {
    destroy_device();
    NSDictionary *props = @{
      @kIOHIDReportDescriptorKey: descriptor(width_mm, height_mm),
      @kIOHIDVendorIDKey: @0x1209,
      @kIOHIDProductIDKey: @0xA012,
      @kIOHIDProductKey: @"Apollo Touch",
      @kIOHIDManufacturerKey: @"Apollo",
      @kIOHIDTransportKey: @"Virtual",
      @kIOHIDSerialNumberKey: [NSString stringWithUTF8String:uuid],
      @kIOHIDPrimaryUsagePageKey: @0x0D,
      @kIOHIDPrimaryUsageKey: @0x04,
      @"displayUUID": [NSString stringWithUTF8String:uuid],
      @"Built-In": @NO,
    };
    device = IOHIDUserDeviceCreateWithProperties(kCFAllocatorDefault, (__bridge CFDictionaryRef) props, 0);
    if (!device) {
      snprintf(error, error_len, "IOHIDUserDeviceCreateWithProperties failed (needs com.apple.developer.hid.virtual.device)");
      return false;
    }
    IOHIDUserDeviceRegisterGetReportBlock(device, ^IOReturn(IOHIDReportType type, uint32_t report_id, uint8_t *report, CFIndex *length) {
      if (report_id == 2 && *length >= 2) {
        report[0] = 2;
        report[1] = max_contacts;
        *length = 2;
        return kIOReturnSuccess;
      }
      return kIOReturnUnsupported;
    });
    dispatch_semaphore_t cancelled = dispatch_semaphore_create(0);
    device_cancelled = cancelled;
    IOHIDUserDeviceSetCancelHandler(device, ^{
      dispatch_semaphore_signal(cancelled);
    });
    IOHIDUserDeviceSetDispatchQueue(device, queue);
    IOHIDUserDeviceActivate(device);
    // WindowServer attaches the digitizer a moment later and drops contacts that arrive before that
    usleep(300000);
    return true;
  }

  void send_frame(const frame_msg_t &f) {
    if (!device) {
      return;
    }
    hid_report_t r {};
    r.report_id = 1;
    r.count = std::min<uint8_t>(f.count, max_contacts);
    for (int i = 0; i < r.count; i++) {
      const auto &c = f.contacts[i];
      r.c[i] = {(uint8_t) (c.flags & 3), (uint8_t) (c.id & 0x7f), c.x, c.y, c.width, c.height};
    }
    IOReturn e = IOHIDUserDeviceHandleReportWithTimeStamp(device, mach_absolute_time(), (const uint8_t *) &r, sizeof r);
    if (e != kIOReturnSuccess) {
      fprintf(stderr, "apollo-touchd: report failed 0x%x\n", e);
    }
  }

  frame_msg_t frame_of(std::initializer_list<contact_t> contacts) {
    frame_msg_t f {touch_protocol::frame, (uint8_t) contacts.size(), {}};
    int i = 0;
    for (auto &c : contacts) {
      f.contacts[i++] = c;
    }
    return f;
  }

  uint16_t at(double v) {
    return (uint16_t) std::lround(std::clamp(v, 0.0, 1.0) * axis_max);
  }

  int selftest(const char *uuid, double hold) {
    char error[128];
    if (!create_device(uuid, 267, 120, error, sizeof error)) {
      fprintf(stderr, "%s\n", error);
      return 1;
    }
    printf("touchscreen made for display %s\n", uuid);
    fflush(stdout);
    usleep(1500000);
    const uint16_t size = at(0.01);
    // tap in the middle
    send_frame(frame_of({{1, 3, at(0.5), at(0.5), size, size}}));
    usleep(60000);
    send_frame(frame_of({{1, 0, at(0.5), at(0.5), size, size}}));
    send_frame(frame_of({}));
    printf("tap\n");
    usleep(700000);
    // one-finger drag from left to right
    for (int s = 0; s <= 30; s++) {
      send_frame(frame_of({{2, 3, at(0.2 + s * 0.02), at(0.6), size, size}}));
      usleep(16000);
    }
    send_frame(frame_of({{2, 0, at(0.8), at(0.6), size, size}}));
    send_frame(frame_of({}));
    printf("drag\n");
    usleep(700000);
    // two-finger spread
    for (int s = 0; s <= 30; s++) {
      double d = 0.05 + s * 0.008;
      send_frame(frame_of({{3, 3, at(0.5 - d), at(0.5), size, size}, {4, 3, at(0.5 + d), at(0.5), size, size}}));
      usleep(16000);
    }
    send_frame(frame_of({{3, 0, at(0.26), at(0.5), size, size}, {4, 0, at(0.74), at(0.5), size, size}}));
    send_frame(frame_of({}));
    printf("pinch\n");
    fflush(stdout);
    usleep((useconds_t) (hold * 1e6));
    destroy_device();
    return 0;
  }

  void reply(bool ok, const char *error) {
    reply_msg_t r {};
    r.ok = ok;
    if (error) {
      strlcpy(r.error, error, sizeof r.error);
    }
    send(helper_fd, &r, sizeof r, 0);
  }

  int serve() {
    // A datagram socket gives no end-of-file when Apollo goes, so also watch for its exit
    dispatch_source_t parent = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, getppid(), DISPATCH_PROC_EXIT, queue);
    dispatch_source_set_event_handler(parent, ^{
      _exit(0);  // the device goes with the process
    });
    dispatch_resume(parent);
    if (getppid() == 1) {
      return 0;
    }
    uint8_t buf[512];
    for (;;) {
      ssize_t n = recv(helper_fd, buf, sizeof buf, 0);
      if (n <= 0) {
        break;  // Apollo closed the socket or died
      }
      switch (buf[0]) {
        case touch_protocol::create:
          if (n >= (ssize_t) sizeof(create_msg_t)) {
            create_msg_t m;
            memcpy(&m, buf, sizeof m);
            m.display_uuid[sizeof m.display_uuid - 1] = 0;
            char error[128] = {};
            bool ok = create_device(m.display_uuid, m.width_mm, m.height_mm, error, sizeof error);
            reply(ok, ok ? nullptr : error);
          }
          break;
        case touch_protocol::frame:
          if (n >= 2) {
            frame_msg_t f {};
            memcpy(&f, buf, std::min<size_t>(n, sizeof f));
            send_frame(f);
          }
          break;
        case touch_protocol::destroy:
          destroy_device();
          reply(true, nullptr);
          break;
      }
    }
    destroy_device();
    return 0;
  }
}  // namespace

int main(int argc, char **argv) {
  @autoreleasepool {
    queue = dispatch_queue_create("apollo-touchd", DISPATCH_QUEUE_SERIAL);
    if (argc >= 4 && !strcmp(argv[1], "--attach")) {
      char error[128];
      if (!create_device(argv[2], 267, 120, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 1;
      }
      printf("touchscreen made for display %s\n", argv[2]);
      fflush(stdout);
      usleep((useconds_t) (atof(argv[3]) * 1e6));
      destroy_device();
      return 0;
    }
    if (argc >= 6 && !strcmp(argv[1], "--tap")) {
      // --tap <uuid> <x> <y> <seconds> [wait]: after wait seconds (1.5), one finger down at x, y (0 to 1) for that long
      char error[128];
      if (!create_device(argv[2], 267, 120, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 1;
      }
      usleep((useconds_t) ((argc > 6 ? atof(argv[6]) : 1.5) * 1e6));
      const uint16_t size = at(0.01), x = at(atof(argv[3])), y = at(atof(argv[4]));
      send_frame(frame_of({{1, 3, x, y, size, size}}));
      usleep((useconds_t) (atof(argv[5]) * 1e6));
      send_frame(frame_of({{1, 0, x, y, size, size}}));
      send_frame(frame_of({}));
      printf("tap at %s, %s\n", argv[3], argv[4]);
      usleep(1000000);
      destroy_device();
      return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "--selftest")) {
      return selftest(argv[2], argc > 3 ? atof(argv[3]) : 2.0);
    }
    return serve();
  }
}
