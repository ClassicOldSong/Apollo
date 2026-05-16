#!/bin/sh

# User Service
mkdir -p ~/.config/systemd/user
cp "/app/share/sunshine/systemd/user/sunshine.service" "$HOME/.config/systemd/user/sunshine.service"
echo "Sunshine User Service has been installed."
echo "Use [systemctl --user enable sunshine] once to autostart Sunshine on login."

# Load host kernel modules. EVDI still must be installed on the host first, for
# example: sudo apt install evdi-dkms libevdi1
UHID=$(cat /app/share/sunshine/modules-load.d/60-sunshine.conf)
echo "Enabling DS5 emulation and Apollo Ubuntu virtual display support."
flatpak-spawn --host pkexec sh -c "echo '$UHID' > /etc/modules-load.d/60-sunshine.conf"
flatpak-spawn --host pkexec modprobe uhid
flatpak-spawn --host pkexec modprobe evdi || echo "warning: evdi module is not available on the host. Install evdi-dkms and reboot if virtual displays fail."

# Udev rule
UDEV=$(cat /app/share/sunshine/udev/rules.d/60-sunshine.rules)
echo "Configuring mouse permission."
flatpak-spawn --host pkexec sh -c "echo '$UDEV' > /etc/udev/rules.d/60-sunshine.rules"
echo "Restart computer for mouse permission to take effect."
