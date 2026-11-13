# Apollo on macOS

Apollo's features on a macOS host: Artemis's virtual display, the client's resolution in HiDPI, the Mac's own
displays off for the stream, and macOS 27's native touch (direct touches, as on a touchscreen Mac).

Tested on an M1 Mac mini (macOS 27.2.1 and 27.3 beta 26C5035r) with Artemis on Android phones.

## Build (on the Mac)

```sh
brew install boost cmake miniupnpc ninja node openssl@3 opus pkg-config
git submodule update --init --recursive
O=/opt/homebrew/opt/openssl@3
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF \
  -DOPENSSL_ROOT_DIR=$O -DOPENSSL_INCLUDE_DIR=$O/include \
  -DOPENSSL_CRYPTO_LIBRARY=$O/lib/libcrypto.dylib -DOPENSSL_SSL_LIBRARY=$O/lib/libssl.dylib
ninja -C build
tools/macos/touchd/build.sh            # build/ApolloTouch.app, found next to build/sunshine
```

## Virtual display (`src/platform/macos/virtual_display.mm`)

- A launch with `virtualDisplay=1` (Artemis's setting, the built-in "Virtual Display" app, or the client's
  "always use virtual display") makes a `CGVirtualDisplay` for that client.
- HiDPI: the backing store is the client's pixel size and the UI is half that in points
  (`macos_vdisplay_scale = 2`, the default; `1` gives one point per pixel).
- macOS 27.2 limits found by testing: a virtual display needs a panel of about 120 mm on its short side or
  it never comes online, and gets its HiDPI mode only when that mode is at least about 525 points high.
  So a 2244x1008 client gets a 2404x1080 display (1202x540 points) and capture scales it to 2244x1008.
- The display's identity (serial) includes its size, because macOS reapplies the mode it last used for an
  identity and a display whose remembered mode is gone never comes online.
- `dd_configuration_option = ensure_only_display` (or `isolated_virtual_display_option`) turns the other
  displays off for the stream, through SkyLight's `SLSConfigureDisplayEnabled`. A copy of the host
  (`sunshine --display-guard <pid> <ids>`, its own session) turns them back on if the host dies:
  `kCGConfigureForAppOnly` is not undone on exit on 27.2.
- Quitting the session restores the displays first, then removes the virtual display.
- `tools/macos/vdtool.mm` exercises all of this by hand.

## Capture (`src/platform/macos/sc_video.mm`)

- ScreenCaptureKit, preferred over AVFoundation. `AVCaptureScreenInput` gets no frames from a virtual display
  because `CGDisplayCopyDisplayMode()` returns NULL for a `CGVirtualDisplay` in the process that made it.
- ScreenCaptureKit sends nothing while the screen is still, so the last frame is repeated every 100 ms;
  without that the end of a session waits for a frame that never comes.
- While the Mac is locked (as after a restart with automatic login) displays are not shareable at all:
  capture sends black until they are, so the host still starts and the encoder test still passes.
- A host that crashes can leave its virtual display behind, and macOS refuses a second display with the same
  vendor, product and serial, so creation falls back to a neighbouring serial.

## Touch (`touchscreen.mm`, `touch_protocol.h`, `tools/macos/touchd`)

What macOS 27 does (read from the 27.2 system, not documented by Apple):

- WindowServer runs iPadOS's BackBoard direct touch processor. SkyLight's `WSHIDDisplayEventListener`
  watches HID services conforming to digitizer page 0x0D usage 4 (touch screen) or 1, reads their
  `displayUUID` property (a CoreGraphics display UUID string) and marks that display touch capable
  (`NSScreen.touchCapabilities`). Their contacts are delivered to apps on that display as direct touches.
  This is how Sidecar feeds an iPad's touches in (`SidecarDisplayAgent`).
- So Apollo makes a virtual HID multi-touch screen whose `displayUUID` is the streamed display's UUID and
  feeds it the client's touches (pen too, as a finger). No touch is turned into a click.
- Making a virtual HID device needs `com.apple.developer.hid.virtual.device`, which AMFI only allows with a
  provisioning profile that grants it; root does not help. So the device lives in `ApolloTouch.app`
  (`apollo-touchd`), started by Apollo with a datagram socket on fd 3; it exits, taking the device with it,
  when Apollo goes.
- Apollo advertises touch to the client only if the helper can make a device; otherwise the client keeps
  turning touch into mouse input.
- Signing: the App ID's HID Virtual Device capability is **development only** (a Developer ID profile leaves
  the entitlement out), so the helper is signed with an Apple Development certificate and a macOS App
  Development profile that lists the Mac it runs on:
  `tools/macos/touchd/build.sh --sign "<Apple Development identity>" --profile <file.provisionprofile>`.
  The bundle id is taken from the profile. Each user needs their own Apple Developer account for this until
  Apple grants the capability for distribution; without a signed helper touch falls back to the mouse.
- Contacts carry Tip Switch, In Range and **Confidence**. Without Confidence the HID driver types each
  contact as a hand instead of a finger; AppKit apps still take those, but the Dock (and other system UI)
  ignores every contact that is not a finger.
- Contact ids are reused like a real touchscreen's: BackBoard uses them as path indexes and drops any contact
  whose id is 30 or more.
- WindowServer attaches a new digitizer a moment after it appears and drops contacts sent before that, so
  the helper waits briefly after making the device.
- An activated `IOHIDUserDevice` has to be cancelled, and the cancel finished, before its last release, or
  the process aborts.
- macOS 27.3 adds touch behaviour of its own on top, such as the traffic-light zoom
  (`NSStoplightZoomInteraction`); it works over Apollo unchanged.
- Test: `apollo-touchd --selftest <display uuid>` plays a tap, a drag and a pinch, `--tap <uuid> <x> <y>
  <seconds>` holds one finger at a point, `--attach <uuid> <seconds>` only makes the device;
  `tools/macos/touchprobe/touchprobe.mm` logs what an app receives (direct touch events, gesture recognizers,
  `NSScreen.touchCapabilities`).

## Mouse

Absolute mouse follows the streamed display (the virtual one when there is one) and maps capture pixels to
points per event.
