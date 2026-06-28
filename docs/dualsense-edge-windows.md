# DualSense Edge Buttons on Windows

## Problem

Moonlight clients can send independent paddle button flags over the Sunshine
controller protocol, but Apollo's Windows backend currently emulates only
Xbox 360 and DualShock 4 controllers through ViGEmBus. Neither target has
independent DualSense Edge Fn or rear paddle buttons, so those inputs are lost
at the Windows virtual controller layer.

The existing protocol path already preserves the bits:

- `PADDLE1 = 0x010000`
- `PADDLE2 = 0x020000`
- `PADDLE3 = 0x040000`
- `PADDLE4 = 0x080000`

`src/input.cpp` combines `buttonFlags | (buttonFlags2 << 16)` before calling
the platform backend. The missing piece is a Windows virtual device that can
represent those bits.

## Relevant Upstream Context

- `ClassicOldSong/Apollo#805`: DualSense support is currently Linux-only because
  Apollo has no Windows DS5 emulation path.
- `ClassicOldSong/Apollo#851`: upstream confirmed the public/open ViGEmBus path
  does not provide a usable DualSense emulation target, despite confusing third
  party claims about DS5 support.
- `ClassicOldSong/Apollo#957`: USB passthrough is considered feasible but too
  much work; the preferred direction is forwarding/reconstructing raw HID rather
  than tunneling arbitrary USB.
- `ClassicOldSong/Apollo#1060`: VirtualHere/USB passthrough remains a recurring
  request, but upstream continues to point out the driver, signing, compatibility,
  and maintenance cost.
- `ClassicOldSong/Apollo#1237`: remapped handheld back buttons should arrive as
  normal A/B/etc. That case is different from DualSense Edge, where SDL exposes
  the extra controls as separate paddle-style controller buttons.
- `moonlight-stream/moonlight-qt#1881`: Moonlight maintainers explicitly describe
  today's protocol surface as basic buttons, paddles, sticks, triggers, motion,
  battery, and touchpads. Full device-specific Steam Input style behavior needs a
  larger HID passthrough feature, but paddles are already in scope for the
  existing controller protocol.
- `moonlight-stream/moonlight-qt#1329`: full PlayStation feature parity is larger
  than this button-preservation path. Maintainer comments describe most standard
  controller features as already present, while sound and adaptive triggers remain
  separate SDL/protocol work.
- `LizardByte/Sunshine#3468` and `LizardByte/Sunshine#3600`: Linux DualSense
  support moved from a uinput-only shape toward UHID because native DS5 games can
  reject or mis-map virtual devices that do not look enough like a real HID
  DualSense. That reinforces exposing a real Edge-shaped HID device for Windows
  rather than presenting another generic gamepad.
- `LizardByte/Sunshine#3527` and `LizardByte/Sunshine#4948`: a WinUHid-based
  Windows DualSense backend is an attractive long-term direction, but the PR was
  closed while driver distribution/signing remained unresolved. Maintainers noted
  that WinUHid currently lacks a ready prebuilt installer, and Cameron Gutman
  clarified that broad architecture support would require WHQL/EV signing. This
  branch therefore keeps `usbip-win2` as the currently testable signed-driver path
  while treating WinUHid as a future cleaner backend candidate.
- `libsdl-org/SDL#13835`: SDL maintainers recommend probing the specific
  controller buttons an app needs with `SDL_GamepadHasButton()`/
  `SDL_GameControllerHasButton()`, which is why raw Edge button count is treated
  as a diagnostic prerequisite rather than proof that the controller-button layer
  is usable.
- `libsdl-org/SDL#14544`: a reported DualSense Edge duplicate-button mapping
  regression was traced to an application mapping-layer issue, while SDL's own
  test controller behaved correctly. This reinforces normalizing stale app-level
  paddle mappings before trusting them.
- `libsdl-org/SDL#14581`: Edge motion support can depend on low-level HID access
  on Linux. That does not block the paddle/Fn transport path, but it explains why
  motion remains a follow-up validation item rather than part of the first
  Windows button-preservation proof.
- Sony's PC/Mac/mobile DualSense Edge support page documents that feature
  availability varies by device and connection type. That supports keeping audio,
  headset, haptics, and adaptive trigger behavior outside this first proof while
  still preserving the controller's independent button inputs.
- Sony's PlayStation Accessories PC support page documents that Windows can
  update DualSense Edge firmware and customize Edge profiles/settings through
  Sony's app. That is useful product context, but profile editing is separate
  from this streaming path; this branch only preserves live controller state that
  Moonlight already sends over the existing protocol.
- Steam Input reports show a real user-facing problem when Edge expanded buttons
  default to face-button behavior globally. The clean streaming fix should
  preserve distinct Edge inputs rather than remap them to A/B/X/Y, DS4, or X360
  buttons.
- Community experiments exist, especially `CTM-Bridge/CTM-USBIP` and
  `sh00bx/Apollo`, but they should be treated as research inputs rather than a
  ready-to-merge Apollo patch.

## DualSense Edge Report Bits

The Linux `hid-playstation` driver documents DualSense Edge extra buttons in
the DualSense input report's third button byte:

- bit 4: Fn left
- bit 5: Fn right
- bit 6: left rear paddle
- bit 7: right rear paddle

The same byte-level order is visible in `hhd-dev/hwinfo`'s public
`ps5edge_LFnRFnLBRB.txt` hardware capture: byte 10 transitions in order
`0x10`, `0x20`, `0x40`, `0x80` when left Fn, right Fn, left rear, and right
rear are pressed.

The Edge HID report descriptor embedded in this branch was checked against the
same public hardware dump at `hhd-dev/hwinfo@d9508123`: the dump's `R: 389`
descriptor line matches `DUALSENSE_EDGE_HID_REPORT_DESCRIPTOR` byte-for-byte.
The same repo's `ps5_edge_sample.pcapng` USB capture includes a
`wTotalLength=0x00e3` configuration descriptor with four interfaces and the
gamepad HID function on interface 3. The companion udev dump also confirms the
wired device identity `054c:0df2`, product string `DualSense Edge Wireless
Controller`, USB 2.00 high-speed operation, and 500 mA max power. Windows USB
tree captures and third-party DS5 filter drivers also show the wired HID path
as `MI_03`. Apollo therefore advertises the captured four-interface USB
configuration so Windows creates the same `MI_03` HID child device, but only
implements the gamepad HID input/output path that controller streaming needs.

SDL2's DualSense Edge HIDAPI path exposes these as raw joystick buttons 17-20,
and its generated controller mapping uses:

- `paddle1:b20`
- `paddle2:b19`
- `paddle3:b18`
- `paddle4:b17`

Apollo should preserve that SDL/Sunshine paddle order unless hardware testing
proves it produces the wrong physical labels.

## Existing ViGEm Backend Limitation

`src/platform/windows/input.cpp` uses ViGEmClient:

- `vigem_target_x360_alloc()`
- `vigem_target_ds4_alloc()`
- `vigem_target_x360_update()`
- `vigem_target_ds4_update_ex()`

Apollo's vendored ViGEmClient headers do not expose a DS5 target. Adding a fake
mapping from Edge paddles to DS4 or X360 buttons would not be a clean fix,
because games still would not see independent Edge controls.

## Experimental Windows Backend

This branch adds an opt-in Windows mode:

```text
gamepad = ds5-edge
```

When selected, Apollo starts a local USB/IP server on `127.0.0.1:3240` and
serves one virtual DualSense Edge device at busid `1-1`. It advertises the
captured composite USB configuration so the gamepad HID interface appears as
`MI_03`, while unsupported audio interfaces are descriptor-only. Unsupported
non-HID endpoint submissions are bounded and rejected without tearing down the
HID controller session. Standard endpoint status and clear-halt controls are
accepted for every endpoint in the captured descriptor, but HID descriptors and
reports are only served for interface 3. Standard interface controls keep all
interfaces at alternate setting 0, so the advertised audio streaming alternate
settings stay inactive until audio support is implemented. Unsupported control
requests return a USB/IP stall status instead of being acknowledged as
successful zero-length transfers. A Windows host still needs `usbip-win2`
attached to that busid so
the signed virtual USB host controller enumerates the device. The backend is
separate from ViGEm, so the existing `auto`, `x360`, and `ds4` paths remain
unchanged.

The first implementation forwards standard buttons, sticks, triggers, and the
four Edge-specific `PADDLE1..4` bits. Touch, motion, battery, and adaptive
trigger state are intentionally left for follow-up hardware testing. Host
output reports are parsed from both interrupt OUT packets and HID control
`SET_REPORT` requests for rumble and lightbar feedback, then forwarded through
Apollo's existing feedback queue. The virtual device also responds to standard
USB enumeration requests such as `SET_ADDRESS`, endpoint `GET_STATUS`, endpoint
`CLEAR_FEATURE(ENDPOINT_HALT)`, and the common DualSense HID feature
`GET_REPORT` requests used by host drivers for calibration (`0x05`), pairing
info (`0x09`), and firmware info (`0x20`). Other advertised Edge feature
reports are sized according to the Edge HID descriptor and return zero-filled
payloads with the requested report ID. HID control `SET_REPORT` is only accepted
for the Edge output report (`type=output`, report ID `0x02`); unsupported HID
input/feature writes still stall instead of being acknowledged as handled.

## Manual Windows Test Flow

### USB/IP Client Setup

Use the current WHLK-signed `vadimgrn/usbip-win2` release as the Windows
USB/IP client. That project provides the signed UDE virtual host controller
that this Apollo backend expects. Avoid validating with the older
`cezanne/usbip-win` client first, because its driver model and behavior differ.

Before installing or updating `usbip-win2`, create a Windows restore point as
recommended by the project. The signed release should not require Windows test
signing mode.

The key evidence to capture during validation is:

- Apollo log lines for the USB/IP listener, import, and Edge paddle/Fn state.
- Moonlight log lines for Edge mapping, arrival support, and optionally each
  per-button debug event.
- `usbip.exe list` / `attach` output.
- Windows PnP device instance IDs showing `VID_054C&PID_0DF2` and the HID child
  path containing `MI_03`.

The following PowerShell snippet captures the USB/IP and Windows PnP side of
that evidence. Run it from an elevated shell after Apollo has logged that the
USB/IP backend is listening:

```powershell
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$dir = Join-Path $env:TEMP "apollo-edge-$stamp"
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$usbipList = Join-Path $dir 'usbip-list.txt'
$usbipAttach = Join-Path $dir 'usbip-attach.txt'
$pnpEdge = Join-Path $dir 'pnp-edge.txt'
$pnpEdgeMi03 = Join-Path $dir 'pnp-edge-mi03.txt'

usbip.exe list -r 127.0.0.1 *> $usbipList
usbip.exe attach -r 127.0.0.1 -b 1-1 *> $usbipAttach

Get-PnpDevice -PresentOnly |
  Where-Object { $_.InstanceId -match 'VID_054C&PID_0DF2' } |
  Format-List Class,FriendlyName,InstanceId,Status |
  Out-File -FilePath $pnpEdge -Encoding utf8

Get-PnpDevice -PresentOnly |
  Where-Object {
    $_.InstanceId -match 'VID_054C&PID_0DF2' -and
    $_.InstanceId -match 'MI_03'
  } |
  Format-List Class,FriendlyName,InstanceId,Status |
  Out-File -FilePath $pnpEdgeMi03 -Encoding utf8

Write-Host "Evidence written to $dir"
```

1. Run this Apollo branch on Windows with:

   ```text
   gamepad = ds5-edge
   ```

2. Start a Moonlight client built from the companion DualSense Edge branch and
   connect the physical DualSense Edge.
3. Confirm Moonlight logs:
   - `DualSense Edge detected with ... SDL joystick buttons` where the count is
     at least `21`.
   - Either `Applied DualSense Edge paddle mappings (...)` or
     `DualSense Edge paddle mappings already present`.
   - If the mapping was `added` rather than `updated`, also expect
     `Reopened DualSense Edge after adding paddle mappings`.
   - Do not continue to the Apollo USB/IP backend if Moonlight logs
     `DualSense Edge paddle mappings are present, but SDL did not expose the
     paddle controller buttons` or
     `DualSense Edge paddle mapping update did not expose the paddle controller
     buttons`; that means SDL still is not reporting the Edge-specific controls
     at the controller-button layer.
   - `DualSense Edge arrival support: ... paddle/Fn=0x000f0000 ...`
4. Confirm Apollo logs:
   - `DualSense Edge USB/IP backend is listening on 127.0.0.1:3240`
   - `Gamepad ... will be DualSense Edge controller (experimental USB/IP)`
   - `Gamepad ... reported DualSense Edge paddle/Fn buttons [000F0000]`
5. In an elevated Windows shell with `usbip-win2`, list and attach the virtual
   controller:

   ```powershell
   usbip.exe list -r 127.0.0.1
   usbip.exe attach -r 127.0.0.1 -b 1-1
   ```

6. Confirm Apollo logs:
   - `DualSense Edge USB/IP: client imported virtual controller`
7. Check Device Manager, `joy.cpl`, Steam Input, or a HID inspection tool for a
   Sony `054c:0df2` DualSense Edge device.
   - Confirm the HID instance path includes `MI_03`. If paddle/Fn buttons are
     present in generic HID/game-controller tools but missing only from
     DS5-specific filter tools, capture the HID path and filter-driver logs.
8. Press each Edge-specific control and verify four distinct host-visible
   inputs:
   - `PADDLE1`: right rear paddle
   - `PADDLE2`: left rear paddle
   - `PADDLE3`: right Fn
   - `PADDLE4`: left Fn

   A quick PowerShell check for the virtual Sony device is:

   ```powershell
   Get-PnpDevice -PresentOnly |
     Where-Object { $_.InstanceId -match 'VID_054C&PID_0DF2' } |
     Select-Object Class,FriendlyName,InstanceId
   ```

9. Optionally, with Moonlight's SDL application debug logging enabled, confirm
   each control changes Moonlight's Edge-state log:
   - `PADDLE1`: `DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)`
   - `PADDLE2`: `DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)`
   - `PADDLE3`: `DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)`
   - `PADDLE4`: `DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)`

   For SDL2 builds, set `SDL_LOGGING=application=debug` before launching
   Moonlight. For example, on Windows PowerShell:

   ```powershell
   $env:SDL_LOGGING = 'application=debug'
   .\Moonlight.exe
   ```

   On macOS from a shell:

   ```sh
   SDL_LOGGING=application=debug /Applications/Moonlight.app/Contents/MacOS/Moonlight
   ```

10. Confirm each control changes Apollo's Edge-state log:
   - `PADDLE1`: `Gamepad ... DualSense Edge paddle/Fn state changed [00010000]`
   - `PADDLE2`: `Gamepad ... DualSense Edge paddle/Fn state changed [00020000]`
   - `PADDLE3`: `Gamepad ... DualSense Edge paddle/Fn state changed [00040000]`
   - `PADDLE4`: `Gamepad ... DualSense Edge paddle/Fn state changed [00080000]`

11. Detach the virtual controller after testing. Use the port number printed by
    `usbip.exe attach`:

    ```powershell
    usbip.exe detach -p 1
    ```

    If multiple stale devices are attached from repeated experiments:

    ```powershell
    usbip.exe detach -all
    ```

If the USB/IP device enumerates but the four controls do not appear, capture
Apollo logs and the HID inspector output first. If Apollo does not log the
advertised `[000F0000]` controller-arrival mask, debug the Moonlight/SDL
mapping path before changing the Windows USB/IP backend. If Apollo logs the
arrival mask but does not log the per-control state changes, enable Moonlight
debug logging and debug the streamed input event path before changing the USB
descriptor. If `usbip.exe attach` succeeds but Windows does not show the
`VID_054C&PID_0DF2` device, capture `usbip-win2` UDE/filter logs next; that
points to USB enumeration or driver behavior rather than Moonlight's mapping.

Do not mark the PR ready until the evidence shows all of the following in the
same end-to-end run:

- Moonlight advertises the Edge paddle/Fn capability mask as `0x000f0000`.
- Apollo receives the same `0x000f0000` arrival mask and logs a distinct state
  change for each `PADDLE1..4` bit.
- `usbip-win2` imports the loopback busid and Windows enumerates
  `VID_054C&PID_0DF2` with an `MI_03` HID child.
- A host-side gamepad or HID inspection tool observes four separate Edge
  paddle/Fn inputs rather than face-button remaps.
- Standard buttons, sticks, and triggers still work through the virtual Edge
  device after the four extra buttons are validated.

## Preferred Architecture

Add a Windows virtual DualSense/Edge backend beside the current ViGEm backend.

The backend should:

1. Be opt-in at first, behind an experimental `gamepad = ds5-edge` or equivalent
   Windows-only setting.
2. Keep X360 and DS4 behavior unchanged.
3. Create a virtual USB/HID controller through a signed, redistributable layer
   rather than requiring a new unsigned kernel driver.
4. Pack Apollo's generic controller state into a DS5 or Edge-compatible input
   report.
5. Map `PADDLE1..4` into the Edge extra-button byte.
6. Parse host output reports and forward rumble, RGB LED, and adaptive trigger
   feedback through Apollo's existing feedback queue where possible.

Candidate transport:

- A local USB/IP server attached by `usbip-win2` is the most practical current
  research path. It uses an existing signed virtual host controller and matches
  the raw-HID-over-reconstructed-device direction discussed upstream.
- WinUHid is likely the cleaner Windows-native target once a signed, easy to
  install driver package exists. Until then, depending on it would leave the proof
  blocked on driver distribution rather than on the Edge button transport itself.

## Implementation Shape

Do not build this by extending Apollo's ViGEmClient submodule with a fake DS5
target. That makes Apollo depend on a dead ViGEm fork and blurs the boundary
between ViGEm and non-ViGEm devices.

Prefer a separate backend boundary:

- `windows/input.cpp` keeps target selection and common dispatch.
- A new `windows/ds5_*` implementation owns DS5/Edge report structs, USB/HID
  descriptors, USB/IP session handling, and output report parsing.
- Common button packing is small and testable:
  - face buttons and d-pad to DS5 `buttons[0]`
  - shoulders, triggers, Create, Options, sticks to `buttons[1]`
  - PS, touchpad click, mic, Edge Fn/paddles to `buttons[2]`

For native Edge buttons, the virtual device must enumerate as an Edge-compatible
device, not just a standard DualSense. A standard DualSense identity
(`054c:0ce6`) is useful as a compatibility fallback, but its public HID report
descriptor exposes only the standard 15 gamepad buttons and leaves the Edge
bits as vendor data. The Edge identity (`054c:0df2`) and Edge HID report
descriptor are the clean target for preserving the four extra controls as
host-visible inputs.

For DualSense Edge mode, `buttons[2]` should include:

```text
if (buttonFlags & PADDLE1) buttons[2] |= 0x80;
if (buttonFlags & PADDLE2) buttons[2] |= 0x40;
if (buttonFlags & PADDLE3) buttons[2] |= 0x20;
if (buttonFlags & PADDLE4) buttons[2] |= 0x10;
```

That follows SDL2's generated mapping order:

- `PADDLE1` from raw button 20: right rear paddle
- `PADDLE2` from raw button 19: left rear paddle
- `PADDLE3` from raw button 18: right Fn
- `PADDLE4` from raw button 17: left Fn

## Milestones

1. Client proof:
   - Moonlight reports `supportedButtonFlags` containing `0x0f0000` for a
     DualSense Edge.
   - Apollo logs controller updates with `buttonFlags2` changing for each
     Edge-specific control.
2. Backend proof:
   - Experimental Windows DS5 backend enumerates as a DualSense-compatible
     device without replacing DS4/X360.
   - Standard buttons, sticks, and triggers work through the virtual Edge input
     report.
   - Touch, motion, battery, and adaptive trigger input are tracked as follow-up
     work; they are not required to prove the Edge paddle/Fn transport path.
3. Edge proof:
   - The virtual device enumerates as Edge-compatible or exposes the Edge
     extra-button bits in a way games and input tools can observe separately.
   - Each Fn/rear paddle toggles a distinct host-visible input.
4. Feedback proof:
   - Host output reports still drive rumble and lightbar feedback where the
     client can apply them.
   - Adaptive trigger feedback remains follow-up work until the Windows Edge
     backend parses and forwards the relevant output report fields.

## Non-Goals

- Do not remap Edge paddles to face buttons as the main solution.
- Do not require Windows test-signing mode or Secure Boot disablement.
- Do not tunnel arbitrary USB devices as part of the first controller-specific
  implementation.
- Do not implement Sony profile editing, firmware update, or PlayStation
  Accessories app compatibility as part of this button-preservation proof.
