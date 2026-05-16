/**
 * @file src/platform/linux/mutter_dbus.h
 * @brief Narrow helpers for GNOME Mutter D-Bus calls used by PipeWire capture.
 */
#pragma once

#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

// platform includes
#include <gio/gio.h>

namespace platf::mutter_dbus {

  constexpr char DBUS_SERVICE[] = "org.freedesktop.DBus";
  constexpr char DBUS_PATH[] = "/org/freedesktop/DBus";
  constexpr char DBUS_INTERFACE[] = "org.freedesktop.DBus";

  constexpr char DISPLAY_CONFIG_SERVICE[] = "org.gnome.Mutter.DisplayConfig";
  constexpr char DISPLAY_CONFIG_PATH[] = "/org/gnome/Mutter/DisplayConfig";

  constexpr char SCREENCAST_SERVICE[] = "org.gnome.Mutter.ScreenCast";
  constexpr char SCREENCAST_PATH[] = "/org/gnome/Mutter/ScreenCast";

  constexpr char REMOTE_DESKTOP_SERVICE[] = "org.gnome.Mutter.RemoteDesktop";
  constexpr char REMOTE_DESKTOP_PATH[] = "/org/gnome/Mutter/RemoteDesktop";

  constexpr int QUICK_CALL_TIMEOUT_MS = 1000;
  constexpr int STREAM_CALL_TIMEOUT_MS = 5000;

  struct gerror_deleter_t {
    void operator()(GError *error) const;
  };

  using gerror_ptr = std::unique_ptr<GError, gerror_deleter_t>;

  bool error_is_missing_object(const GError *error);
  bool name_has_owner(GDBusConnection *bus, const char *name, int timeout_ms);
  GVariant *call_sync(
    GDBusConnection *bus,
    const char *destination,
    const char *path,
    const char *interface,
    const char *method,
    GVariant *parameters,
    const GVariantType *reply_type,
    int timeout_ms,
    GError **error
  );
  bool call_no_args(
    GDBusConnection *bus,
    const char *destination,
    const char *path,
    const char *interface,
    const char *method,
    int timeout_ms,
    const char *log_context
  );

  uint32_t uint32_from_variant(GVariant *value);
  std::int64_t int64_from_variant(GVariant *value);
  std::optional<std::string> child_string(GVariant *variant, guint index);
  std::string string_property(GVariant *properties, const char *name);

}  // namespace platf::mutter_dbus

#endif
