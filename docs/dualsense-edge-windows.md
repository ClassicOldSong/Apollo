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
- `ClassicOldSong/Apollo#957`: USB passthrough is considered feasible but too
  much work; the preferred direction is forwarding/reconstructing raw HID rather
  than tunneling arbitrary USB.
- `ClassicOldSong/Apollo#1237`: remapped handheld back buttons should arrive as
  normal A/B/etc. That case is different from DualSense Edge, where SDL exposes
  the extra controls as separate paddle-style controller buttons.
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
serves one virtual DualSense Edge device at busid `1-1`. A Windows host still
needs `usbip-win2` attached to that busid so the signed virtual USB host
controller enumerates the device. The backend is separate from ViGEm, so the
existing `auto`, `x360`, and `ds4` paths remain unchanged.

The first implementation forwards standard buttons, sticks, triggers, and the
four Edge-specific `PADDLE1..4` bits. Touch, motion, battery, and adaptive
trigger state are intentionally left for follow-up hardware testing. Host
output reports are parsed for rumble and lightbar feedback and forwarded
through Apollo's existing feedback queue. The virtual device also responds to
the common DualSense HID feature `GET_REPORT` requests used by host drivers
for calibration (`0x05`), pairing info (`0x09`), and firmware info (`0x20`).
Other advertised Edge feature reports are sized according to the Edge HID
descriptor and return zero-filled payloads with the requested report ID.

## Manual Windows Test Flow

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
8. Press each Edge-specific control and verify four distinct host-visible
   inputs:
   - `PADDLE1`: right rear paddle
   - `PADDLE2`: left rear paddle
   - `PADDLE3`: right Fn
   - `PADDLE4`: left Fn
9. Confirm each control changes Moonlight's Edge-state log:
   - `PADDLE1`: `DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)`
   - `PADDLE2`: `DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)`
   - `PADDLE3`: `DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)`
   - `PADDLE4`: `DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)`
10. Confirm each control changes Apollo's Edge-state log:
   - `PADDLE1`: `Gamepad ... DualSense Edge paddle/Fn state changed [00010000]`
   - `PADDLE2`: `Gamepad ... DualSense Edge paddle/Fn state changed [00020000]`
   - `PADDLE3`: `Gamepad ... DualSense Edge paddle/Fn state changed [00040000]`
   - `PADDLE4`: `Gamepad ... DualSense Edge paddle/Fn state changed [00080000]`

If the USB/IP device enumerates but the four controls do not appear, capture
Apollo logs and the HID inspector output first. If Apollo does not log the
advertised `[000F0000]` controller-arrival mask, debug the Moonlight/SDL
mapping path before changing the Windows USB/IP backend. If Apollo logs the
arrival mask but does not log the per-control state changes, debug the streamed
input event path before changing the USB descriptor.

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
   - Standard buttons, sticks, triggers, motion, and touchpad still work.
3. Edge proof:
   - The virtual device enumerates as Edge-compatible or exposes the Edge
     extra-button bits in a way games and input tools can observe separately.
   - Each Fn/rear paddle toggles a distinct host-visible input.
4. Feedback proof:
   - Host output reports still drive rumble, lightbar, and adaptive trigger
     feedback where the client can apply them.

## Non-Goals

- Do not remap Edge paddles to face buttons as the main solution.
- Do not require Windows test-signing mode or Secure Boot disablement.
- Do not tunnel arbitrary USB devices as part of the first controller-specific
  implementation.
