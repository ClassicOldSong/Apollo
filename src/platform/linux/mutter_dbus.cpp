/**
 * @file src/platform/linux/mutter_dbus.cpp
 * @brief Narrow helpers for GNOME Mutter D-Bus calls used by PipeWire capture.
 */

#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <algorithm>
#include <cstring>

// local includes
#include "mutter_dbus.h"
#include "src/logging.h"

namespace platf::mutter_dbus {

  void gerror_deleter_t::operator()(GError *error) const {
    if (error) {
      g_error_free(error);
    }
  }

  bool error_is_missing_object(const GError *error) {
    return error && error->message && std::strstr(error->message, "Object does not exist");
  }

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
  ) {
    return g_dbus_connection_call_sync(
      bus,
      destination,
      path,
      interface,
      method,
      parameters,
      reply_type,
      G_DBUS_CALL_FLAGS_NONE,
      timeout_ms,
      nullptr,
      error
    );
  }

  bool call_no_args(
    GDBusConnection *bus,
    const char *destination,
    const char *path,
    const char *interface,
    const char *method,
    int timeout_ms,
    const char *log_context
  ) {
    if (!bus || !path || !*path) {
      return true;
    }

    GError *raw_error = nullptr;
    auto result = call_sync(
      bus,
      destination,
      path,
      interface,
      method,
      nullptr,
      nullptr,
      timeout_ms,
      &raw_error
    );
    gerror_ptr dbus_error(raw_error);
    if (!result) {
      if (error_is_missing_object(dbus_error.get())) {
        BOOST_LOG(debug) << log_context << ' ' << method
                         << " skipped because the object already disappeared.";
        return true;
      }
      BOOST_LOG(error) << log_context << ' ' << method
                       << " failed: " << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    g_variant_unref(result);
    return true;
  }

  bool name_has_owner(GDBusConnection *bus, const char *name, int timeout_ms) {
    GError *raw_error = nullptr;
    auto result = call_sync(
      bus,
      DBUS_SERVICE,
      DBUS_PATH,
      DBUS_INTERFACE,
      "NameHasOwner",
      g_variant_new("(s)", name),
      G_VARIANT_TYPE("(b)"),
      timeout_ms,
      &raw_error
    );
    gerror_ptr dbus_error(raw_error);
    if (!result) {
      BOOST_LOG(debug) << "Unable to query D-Bus owner for " << name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    gboolean has_owner = false;
    g_variant_get(result, "(b)", &has_owner);
    g_variant_unref(result);
    return has_owner;
  }

  uint32_t uint32_from_variant(GVariant *value) {
    if (!value) {
      return 0;
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
      return g_variant_get_uint32(value);
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
      return static_cast<uint32_t>(std::max(0, g_variant_get_int32(value)));
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64)) {
      return static_cast<uint32_t>(g_variant_get_uint64(value));
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
      return static_cast<uint32_t>(std::max<int64_t>(0, g_variant_get_int64(value)));
    }
    return 0;
  }

  std::int64_t int64_from_variant(GVariant *value) {
    if (!value) {
      return -1;
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
      return g_variant_get_int32(value);
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
      return g_variant_get_uint32(value);
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
      return g_variant_get_int64(value);
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64)) {
      return static_cast<std::int64_t>(g_variant_get_uint64(value));
    }
    return -1;
  }

  std::optional<std::string> child_string(GVariant *variant, guint index) {
    auto child = g_variant_get_child_value(variant, index);
    if (!child) {
      return std::nullopt;
    }

    std::optional<std::string> result;
    if (g_variant_is_of_type(child, G_VARIANT_TYPE_STRING)) {
      result = g_variant_get_string(child, nullptr);
    }
    g_variant_unref(child);
    return result;
  }

  std::string string_property(GVariant *properties, const char *name) {
    auto value = g_variant_lookup_value(properties, name, G_VARIANT_TYPE_STRING);
    if (!value) {
      return {};
    }

    std::string result = g_variant_get_string(value, nullptr);
    g_variant_unref(value);
    return result;
  }

}  // namespace platf::mutter_dbus

#endif
