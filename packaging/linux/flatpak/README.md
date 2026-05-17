# Apollo Ubuntu Flatpak

The Flatpak manifest is maintained for experimentation and future distribution of Apollo Ubuntu.
The `.deb` package remains the recommended Ubuntu install path because the default virtual display backend depends on EVDI, which is a host kernel module and cannot be bundled inside Flatpak.

## Host Prerequisites

```bash
sudo apt update
sudo apt install evdi-dkms libevdi1 flatpak flatpak-builder
sudo modprobe evdi
```

If Secure Boot is enabled, the host must trust the DKMS signing key before EVDI
can load. Enroll the MOK key during the `evdi-dkms` install prompt, then reboot
and complete the blue MOK Manager enrollment screen. If the prompt was missed:

```bash
sudo update-secureboot-policy --enroll-key
sudo dkms autoinstall
sudo reboot
```

After reboot, verify:

```bash
sudo modprobe evdi
lsmod | grep evdi
```

`Required key not available` or `Key was rejected by service` means Secure Boot
is still blocking the EVDI module.

## Build

Configure the manifest from the repository root:

```bash
cmake -B build-flatpak-manifest -S . \
  -DSUNSHINE_CONFIGURE_ONLY=ON \
  -DSUNSHINE_CONFIGURE_FLATPAK_MAN=ON \
  -DSUNSHINE_BUILD_FLATPAK=ON
```

If `package-lock.json` changes, refresh the checked-in npm source list before
configuring the manifest:

```bash
python3 packaging/linux/flatpak/deps/flatpak-builder-tools/npm/flatpak-npm-generator.py \
  package-lock.json -o generated-sources.json
```

Build the Flatpak:

```bash
flatpak-builder --force-clean --repo=repo flatpak-build build-flatpak-manifest/io.github.primezx.ApolloUbuntu.yml
flatpak build-bundle repo ApolloUbuntu-x86_64.flatpak io.github.primezx.ApolloUbuntu
```

## Install

```bash
flatpak install --user ./ApolloUbuntu-x86_64.flatpak
flatpak run --command=additional-install.sh io.github.primezx.ApolloUbuntu
flatpak run io.github.primezx.ApolloUbuntu
```

`additional-install.sh` installs the user service, host input rules, and module-load file through `flatpak-spawn --host pkexec`.

## Limitations

- EVDI must be installed and loadable on the host outside Flatpak.
- Kernel updates may require `sudo dkms autoinstall`, `sudo modprobe evdi`, or a reboot.
- Validate GNOME Wayland, PipeWire capture, and Moonlight streaming before publishing a Flatpak build.
