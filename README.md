# Apollo Ubuntu

Apollo Ubuntu is a Linux-focused fork of [Apollo](https://github.com/ClassicOldSong/Apollo) for Ubuntu GNOME Wayland hosts.
It keeps Apollo's Moonlight/Artemis streaming workflow while maintaining the Linux virtual display and packaging work in this repository instead of trying to merge those Ubuntu-specific changes back into the Windows-first parent.

## What This Fork Provides

- GNOME Wayland virtual display streaming for Ubuntu.
- EVDI-backed real virtual monitors so streamed sessions do not mirror the physical display.
- Mutter ScreenCast/PipeWire capture for smooth frame pacing on GNOME.
- AMD, Intel, and Nvidia encoding through the Linux encoder stack available on the host.
- User service packaging, udev rules, EVDI module loading, and Ubuntu install documentation.
- A cautious upstream-tracking workflow for reviewing ClassicOldSong/Apollo changes before merging them into this Linux fork.

## Supported Host

This branch targets Ubuntu desktop hosts, especially:

- Ubuntu 24.04 LTS or newer.
- Ubuntu 26.04 development/current testing builds.
- GNOME on Wayland.
- A Moonlight-compatible client such as Artemis or Moonlight.

Other Linux distributions may still build, but Ubuntu is the supported release target for this fork.

## Recommended Install

Use the `.deb` artifact from this fork's releases when available.

```bash
sudo apt update
sudo apt install ./ApolloUbuntu*.deb
sudo reboot
```

After reboot:

```bash
systemctl --user enable --now sunshine.service
journalctl --user -u sunshine.service -f
```

Open the web UI:

```text
https://localhost:47990
```

The browser will warn about the self-signed certificate. That is expected for the local web UI.

## Ubuntu Runtime Requirements

The Debian package is intended to install the required runtime dependencies, including EVDI, PipeWire, GIO/GLib, DRM, VAAPI, and input rules. If you are preparing a host manually, install:

```bash
sudo apt update
sudo apt install evdi-dkms libevdi1 pipewire wireplumber
sudo modprobe evdi
```

Verify the virtual display prerequisites:

```bash
lsmod | grep evdi
systemctl --user status pipewire wireplumber
```

If `evdi` is not loaded after a kernel update, reboot or run:

```bash
sudo dkms autoinstall
sudo modprobe evdi
```

### Secure Boot And MOK Enrollment

On systems with UEFI Secure Boot enabled, Ubuntu will only load DKMS-built
kernel modules after the module-signing key has been enrolled through MOK
(`Machine Owner Key`). This is a host firmware/Secure Boot trust step, not an
Apollo setting.

Check Secure Boot state:

```bash
mokutil --sb-state
```

If Secure Boot is enabled, `apt install ./ApolloUbuntu*.deb` may prompt you to
create a one-time MOK enrollment password while `evdi-dkms` is being installed.
After the package install completes, reboot and use the blue MOK Manager screen
to enroll the key:

```text
Enroll MOK -> Continue -> Yes -> enter the one-time password -> Reboot
```

If the prompt was skipped, or if EVDI later fails to load after a kernel update,
re-run the enrollment flow and rebuild the DKMS module:

```bash
sudo update-secureboot-policy --enroll-key
sudo dkms autoinstall
sudo reboot
```

After reboot:

```bash
sudo modprobe evdi
lsmod | grep evdi
```

If `modprobe evdi` reports `Required key not available` or `Key was rejected by
service`, Secure Boot is blocking the EVDI module and MOK enrollment is still
required.

## Build From Source On Ubuntu

Install build dependencies:

```bash
./scripts/linux_build.sh deps
```

Build:

```bash
cmake -B build -G Ninja -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DSUNSHINE_ENABLE_WAYLAND=ON \
  -DSUNSHINE_ENABLE_X11=ON \
  -DSUNSHINE_ENABLE_DRM=ON

cmake --build build -j"$(nproc)"
```

Install locally:

```bash
sudo cmake --install build
sudo setcap cap_sys_admin+p "$(command -v sunshine)"
systemctl --user daemon-reload
systemctl --user enable --now sunshine.service
```

Build a `.deb`:

```bash
cpack -G DEB --config build/CPackConfig.cmake
```

## Flatpak Status

Flatpak packaging is kept in-tree for experimentation and future distribution, but the Ubuntu `.deb` is the recommended install path for the virtual display backend.

EVDI is a host kernel module, so Flatpak cannot fully self-contain virtual display setup. Before running the Flatpak build, install EVDI on the host:

```bash
sudo apt install evdi-dkms libevdi1
sudo modprobe evdi
```

After installing the Flatpak artifact:

```bash
flatpak run --command=additional-install.sh io.github.primezx.ApolloUbuntu
flatpak run io.github.primezx.ApolloUbuntu
```

## Virtual Display Backend

Default:

```text
linux_virtual_display_backend = auto
```

`auto` uses the EVDI monitor plus Mutter ScreenCast/PipeWire backend. That is the supported GNOME Wayland path.

Diagnostic alternatives:

- `mutter`: GNOME Mutter RecordVirtual/PipeWire without EVDI.
- `evdi`: direct EVDI/KMS capture.

For temporary diagnostics only, the environment variable below overrides the config value:

```bash
APOLLO_LINUX_VIRTUAL_BACKEND=evdi systemctl --user restart sunshine.service
```

## Logs And Troubleshooting

Service logs:

```bash
journalctl --user -u sunshine.service -f
```

Virtual display checks:

```bash
lsmod | grep evdi
ls /dev/dri
systemctl --user status pipewire wireplumber
```

If Moonlight cannot see the host, verify the service is running and the web/API ports are listening:

```bash
systemctl --user status sunshine.service
curl -k https://localhost:47990
curl http://localhost:47989/serverinfo
```

## Fork Maintenance

ClassicOldSong/Apollo remains the parent project for Apollo behavior, but this repository is the Ubuntu/Linux release line. Parent changes should be imported deliberately, tested against GNOME Wayland, EVDI, PipeWire, and Moonlight streaming, and then merged into this fork only after Linux compatibility is reviewed.

See [docs/fork-maintenance.md](docs/fork-maintenance.md) for the upstream tracking workflow.

## Credits

This fork builds on:

- [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo)
- [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine)
- [DisplayLink EVDI](https://github.com/DisplayLink/evdi)
- GNOME Mutter ScreenCast and PipeWire

## Support

Use issues in this fork for Ubuntu/Linux virtual display bugs:

```text
https://github.com/primez-x/Apollo-Ubuntu/issues
```
