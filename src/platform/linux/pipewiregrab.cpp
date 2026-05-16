/**
 * @file src/platform/linux/pipewiregrab.cpp
 * @brief GNOME Mutter ScreenCast/PipeWire capture.
 */

#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// lib includes
#include <gio/gio.h>
#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

// local includes
#include "cuda.h"
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/video.h"
#include "vaapi.h"
#include "virtual_display.h"

using namespace std::literals;

namespace platf {
  namespace {
    constexpr char MUTTER_SCREENCAST[] = "org.gnome.Mutter.ScreenCast";
    constexpr char MUTTER_SCREENCAST_PATH[] = "/org/gnome/Mutter/ScreenCast";
    constexpr int MUTTER_DBUS_TIMEOUT_MS = 5000;

    struct gerror_deleter_t {
      void operator()(GError *error) const {
        if (error) {
          g_error_free(error);
        }
      }
    };

	    using gerror_ptr = std::unique_ptr<GError, gerror_deleter_t>;

	    std::string upper_copy(std::string value) {
	      std::transform(std::begin(value), std::end(value), std::begin(value), [](unsigned char c) {
	        return static_cast<char>(std::toupper(c));
	      });
	      return value;
	    }

	    std::optional<std::string> variant_child_string(GVariant *variant, guint index) {
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

	    std::string lookup_string_property(GVariant *properties, const char *name) {
	      auto value = g_variant_lookup_value(properties, name, G_VARIANT_TYPE_STRING);
	      if (!value) {
	        return {};
	      }

	      std::string result = g_variant_get_string(value, nullptr);
	      g_variant_unref(value);
	      return result;
	    }

	    bool is_apollo_monitor_spec(const std::string &connector, const std::string &vendor, const std::string &product, const std::string &display_name) {
	      auto haystack = upper_copy(connector + " " + vendor + " " + product + " " + display_name);
	      return upper_copy(vendor) == "APL" || haystack.find("APOLLO") != std::string::npos || haystack.find("VDISP") != std::string::npos;
	    }

    bool mutter_screencast_available() {
      gerror_ptr error;
      GError *raw_error = nullptr;
      auto bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &raw_error);
      error.reset(raw_error);
      if (!bus) {
        return false;
      }

      raw_error = nullptr;
      auto result = g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", MUTTER_SCREENCAST),
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        nullptr,
        &raw_error
      );
      error.reset(raw_error);
      g_object_unref(bus);

      if (!result) {
        return false;
      }

      gboolean has_owner = false;
      g_variant_get(result, "(b)", &has_owner);
      g_variant_unref(result);
      return has_owner;
    }

    class pipewire_img_t: public img_t {
    public:
      std::vector<std::uint8_t> storage;
    };

    class pipewire_display_t: public display_t {
    public:
      ~pipewire_display_t() override {
        stop();
      }

      int init(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
        this->display_name = display_name;
        mem_type = hwdevice_type;
        width = config.width;
        height = config.height;
        std::uint32_t requested_framerate_override {};
        std::uint32_t virtual_width {};
        std::uint32_t virtual_height {};
        std::uint32_t virtual_fps {};
        const auto virtual_backend = VDISPLAY::virtualDisplayBackend(display_name);
        if ((virtual_backend == VDISPLAY::BACKEND::MUTTER_PIPEWIRE ||
             virtual_backend == VDISPLAY::BACKEND::EVDI_PIPEWIRE) &&
            VDISPLAY::getVirtualDisplayMode(display_name, virtual_width, virtual_height, virtual_fps)) {
          width = static_cast<int>(virtual_width);
          height = static_cast<int>(virtual_height);
          if (virtual_fps) {
            requested_framerate_override = virtual_fps;
          }
        }
        env_width = width;
        env_height = height;
	        auto requested_framerate = requested_framerate_override ? requested_framerate_override : config.framerate;
	        if (requested_framerate >= 1000) {
	          requested_framerate /= 1000;
	        }
	        framerate = std::max<std::uint32_t>(1, requested_framerate);
	        frame_interval = std::chrono::nanoseconds {1s} / framerate;

        if (!start_mutter_stream()) {
          return -1;
        }

        if (!start_pipewire_stream()) {
          return -1;
        }

        return 0;
      }

	      capture_e capture(const push_captured_image_cb_t &push_captured_image_cb, const pull_free_image_cb_t &pull_free_image_cb, bool *) override {
	        capture_diag_at = std::chrono::steady_clock::now();
	        capture_frames = 0;
	        capture_new_frames = 0;
	        capture_repeated_frames = 0;
	        capture_max_wait_ms = 0;
	        capture_max_copy_ms = 0;

	        while (running) {
	          std::shared_ptr<img_t> img;
	          if (!pull_free_image_cb(img)) {
	            return capture_e::interrupted;
	          }

	          bool copied = false;
	          double wait_ms = 0;
	          double copy_ms = 0;
	          {
	            std::unique_lock lock(frame_mutex);
	            auto wanted_generation = consumed_generation;
	            auto wait_start = std::chrono::steady_clock::now();
	            frame_cv.wait_for(lock, frame_interval, [&]() {
	              return !running || latest_generation != wanted_generation;
	            });
	            wait_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wait_start).count();

	            if (!running) {
	              return capture_e::interrupted;
	            }

	            if (!latest_pixels.empty() && latest_generation != consumed_generation) {
	              auto copy_start = std::chrono::steady_clock::now();
	              const auto copy_height = std::min<int>(height, latest_height);
	              const auto copy_width = std::min<int>(width, latest_width);
	              for (int y = 0; y < copy_height; ++y) {
	                auto src = latest_pixels.data() + (static_cast<std::size_t>(y) * latest_stride);
	                auto dst = img->data + (static_cast<std::size_t>(y) * img->row_pitch);
	                std::memcpy(dst, src, static_cast<std::size_t>(copy_width) * 4);
	              }
	              copy_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copy_start).count();
	              img->frame_timestamp = latest_timestamp;
	              consumed_generation = latest_generation;
	              copied = true;
	            }
	          }

	          log_capture_diag(copied, wait_ms, copy_ms);

	          if (!push_captured_image_cb(std::move(img), copied)) {
	            return capture_e::ok;
	          }
        }

        return capture_e::interrupted;
      }

      std::shared_ptr<img_t> alloc_img() override {
        auto img = std::make_shared<pipewire_img_t>();
        img->width = width;
        img->height = height;
        img->pixel_pitch = 4;
        img->row_pitch = width * 4;
        img->storage.assign(static_cast<std::size_t>(img->row_pitch) * height, 0);
        img->data = img->storage.data();
        return img;
      }

      int dummy_img(img_t *img) override {
        if (!img || !img->data) {
          return -1;
        }

        std::memset(img->data, 0, static_cast<std::size_t>(img->row_pitch) * img->height);
        img->frame_timestamp = std::chrono::steady_clock::now();
        return 0;
      }

      std::unique_ptr<avcodec_encode_device_t> make_avcodec_encode_device(pix_fmt_e) override {
#ifdef SUNSHINE_BUILD_VAAPI
        if (mem_type == mem_type_e::vaapi) {
          return va::make_avcodec_encode_device(width, height, false);
        }
#endif

#ifdef SUNSHINE_BUILD_CUDA
        if (mem_type == mem_type_e::cuda) {
          return cuda::make_avcodec_encode_device(width, height, false);
        }
#endif

        return std::make_unique<avcodec_encode_device_t>();
      }

    private:
      static void on_stream_state_changed(void *data, pw_stream_state, pw_stream_state state, const char *stream_error) {
        auto self = static_cast<pipewire_display_t *>(data);
        if (state == PW_STREAM_STATE_ERROR) {
          BOOST_LOG(error) << "GNOME PipeWire stream error: "sv << (stream_error ? stream_error : "unknown");
          self->running = false;
          self->frame_cv.notify_all();
        }
      }

      static void on_stream_param_changed(void *data, uint32_t id, const spa_pod *param) {
        auto self = static_cast<pipewire_display_t *>(data);
        if (!param || id != SPA_PARAM_Format) {
          return;
        }

        spa_video_info_raw video_info {};
        if (spa_format_video_raw_parse(param, &video_info) < 0) {
          return;
        }

        std::lock_guard lock(self->frame_mutex);
        self->pipewire_format = video_info.format;
        if (video_info.size.width > 0 && video_info.size.height > 0) {
          self->latest_width = static_cast<int>(video_info.size.width);
          self->latest_height = static_cast<int>(video_info.size.height);
          self->width = self->latest_width;
          self->height = self->latest_height;
          self->env_width = self->width;
          self->env_height = self->height;
        }
        BOOST_LOG(info) << "GNOME PipeWire capture format "
                        << self->latest_width << 'x' << self->latest_height
                        << " spa_format=" << static_cast<int>(self->pipewire_format);
      }

	      static void on_stream_process(void *data) {
	        auto self = static_cast<pipewire_display_t *>(data);
	        auto buffer = pw_stream_dequeue_buffer(self->stream);
        if (!buffer) {
          return;
        }

        auto spa_buffer = buffer->buffer;
        if (!spa_buffer || spa_buffer->n_datas == 0 || !spa_buffer->datas[0].data || !spa_buffer->datas[0].chunk) {
          pw_stream_queue_buffer(self->stream, buffer);
          return;
        }

        const auto &data0 = spa_buffer->datas[0];
        const auto *chunk = data0.chunk;
	        const auto src_stride = chunk->stride > 0 ? chunk->stride : static_cast<int32_t>(self->width * 4);
	        const auto src = static_cast<const std::uint8_t *>(data0.data) + chunk->offset;
	        const auto frame_width = self->latest_width > 0 ? self->latest_width : self->width;
	        const auto frame_height = self->latest_height > 0 ? self->latest_height : self->height;
	        const auto row_bytes = static_cast<std::size_t>(std::min<int>(std::abs(src_stride), frame_width * 4));
	        const auto copy_start = std::chrono::steady_clock::now();

	        {
	          std::lock_guard lock(self->frame_mutex);
	          self->latest_stride = frame_width * 4;
	          self->latest_pixels.resize(static_cast<std::size_t>(self->latest_stride) * frame_height);
	          for (int y = 0; y < frame_height; ++y) {
	            auto src_row = src + (static_cast<std::size_t>(y) * std::abs(src_stride));
	            auto dst_row = self->latest_pixels.data() + (static_cast<std::size_t>(y) * self->latest_stride);
	            std::memcpy(dst_row, src_row, row_bytes);
	          }
          self->latest_timestamp = std::chrono::steady_clock::now();
	          ++self->latest_generation;
	        }
	        self->frame_cv.notify_all();
	        self->log_process_diag(
	          static_cast<std::size_t>(row_bytes) * frame_height,
	          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copy_start).count(),
	          data0.type,
	          src_stride
	        );

	        pw_stream_queue_buffer(self->stream, buffer);
	      }

	      bool start_mutter_stream() {
        if (VDISPLAY::virtualDisplayBackend(display_name) == VDISPLAY::BACKEND::MUTTER_PIPEWIRE) {
          std::uint32_t backend_node_id {};
          if (!VDISPLAY::getMutterPipeWireNodeId(display_name, backend_node_id)) {
            BOOST_LOG(error) << "Mutter/PipeWire virtual display ["sv << display_name
                             << "] has no PipeWire node; refusing capture fallback."sv;
            return false;
          }

          node_id = backend_node_id;
          owns_mutter_session = false;
          BOOST_LOG(info) << "Using backend-owned Mutter/PipeWire node " << node_id
                          << " for virtual display [" << display_name << ']';
          return true;
        }

	        GError *raw_error = nullptr;
	        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &raw_error);
        if (!bus) {
          gerror_ptr dbus_error(raw_error);
          BOOST_LOG(error) << "Unable to connect to session bus for GNOME PipeWire capture: "sv << (dbus_error ? dbus_error->message : "unknown");
          return false;
        }

        GVariantBuilder session_props;
        g_variant_builder_init(&session_props, G_VARIANT_TYPE("a{sv}"));

        auto session_result = g_dbus_connection_call_sync(
          bus,
          MUTTER_SCREENCAST,
          MUTTER_SCREENCAST_PATH,
          "org.gnome.Mutter.ScreenCast",
          "CreateSession",
          g_variant_new("(a{sv})", &session_props),
          G_VARIANT_TYPE("(o)"),
          G_DBUS_CALL_FLAGS_NONE,
          MUTTER_DBUS_TIMEOUT_MS,
          nullptr,
          &raw_error
        );
        if (!session_result) {
          gerror_ptr dbus_error(raw_error);
          BOOST_LOG(error) << "Unable to create GNOME ScreenCast session: "sv << (dbus_error ? dbus_error->message : "unknown");
          return false;
        }

        const char *session_path_raw = nullptr;
        g_variant_get(session_result, "(&o)", &session_path_raw);
        session_path = session_path_raw;
        g_variant_unref(session_result);

        GVariantBuilder props;
        g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&props, "{sv}", "cursor-mode", g_variant_new_uint32(1));
        g_variant_builder_add(&props, "{sv}", "is-recording", g_variant_new_boolean(TRUE));
        g_variant_builder_add(&props, "{sv}", "is-platform", g_variant_new_boolean(TRUE));
	        g_variant_builder_add(&props, "{sv}", "width", g_variant_new_uint32(width));
	        g_variant_builder_add(&props, "{sv}", "height", g_variant_new_uint32(height));
	        g_variant_builder_add(&props, "{sv}", "framerate", g_variant_new_uint32(framerate));

	        const bool record_existing_monitor = VDISPLAY::isEvdiDisplay(display_name);
	        std::string connector_name;
	        if (record_existing_monitor) {
	          connector_name = find_apollo_monitor_connector();
	          if (connector_name.empty()) {
	            BOOST_LOG(error) << "Unable to find Apollo virtual monitor in Mutter DisplayConfig for GNOME PipeWire RecordMonitor."sv;
	            return false;
	          }

	          BOOST_LOG(info) << "GNOME ScreenCast will RecordMonitor connector ["sv << connector_name << "] for ["sv << display_name << ']';
	        }

	        raw_error = nullptr;
	        auto stream_result = g_dbus_connection_call_sync(
	          bus,
	          MUTTER_SCREENCAST,
	          session_path.c_str(),
	          "org.gnome.Mutter.ScreenCast.Session",
	          record_existing_monitor ? "RecordMonitor" : "RecordVirtual",
	          record_existing_monitor ? g_variant_new("(sa{sv})", connector_name.c_str(), &props) : g_variant_new("(a{sv})", &props),
	          G_VARIANT_TYPE("(o)"),
	          G_DBUS_CALL_FLAGS_NONE,
	          MUTTER_DBUS_TIMEOUT_MS,
          nullptr,
          &raw_error
        );
	        if (!stream_result) {
	          gerror_ptr dbus_error(raw_error);
	          BOOST_LOG(error) << "Unable to create GNOME "sv
	                           << (record_existing_monitor ? "monitor"sv : "virtual"sv)
	                           << " ScreenCast stream: "sv
	                           << (dbus_error ? dbus_error->message : "unknown");
	          return false;
	        }

        const char *stream_path_raw = nullptr;
        g_variant_get(stream_result, "(&o)", &stream_path_raw);
        stream_path = stream_path_raw;
        g_variant_unref(stream_result);

        signal_id = g_dbus_connection_signal_subscribe(
          bus,
          MUTTER_SCREENCAST,
          "org.gnome.Mutter.ScreenCast.Stream",
          "PipeWireStreamAdded",
          stream_path.c_str(),
          nullptr,
          G_DBUS_SIGNAL_FLAGS_NONE,
          [](GDBusConnection *, const char *, const char *, const char *, const char *, GVariant *parameters, gpointer user_data) {
            auto self = static_cast<pipewire_display_t *>(user_data);
            guint node = 0;
            g_variant_get(parameters, "(u)", &node);
            {
              std::lock_guard lock(self->node_mutex);
              self->node_id = node;
            }
            self->node_cv.notify_all();
          },
          this,
          nullptr
        );

        if (!call_no_args(session_path, "org.gnome.Mutter.ScreenCast.Session", "Start")) {
          return false;
        }

        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
          while (g_main_context_iteration(nullptr, FALSE)) {
          }
          {
            std::lock_guard lock(node_mutex);
            if (node_id) {
              break;
            }
          }
          std::this_thread::sleep_for(20ms);
        }

        if (!node_id) {
          BOOST_LOG(error) << "Timed out waiting for GNOME PipeWire stream node.";
          return false;
        }

        BOOST_LOG(info) << "GNOME ScreenCast PipeWire node " << node_id << " created.";
	        return true;
	      }

	      std::string find_apollo_monitor_connector() {
	        const auto deadline = std::chrono::steady_clock::now() + 3s;
	        while (std::chrono::steady_clock::now() < deadline) {
	          auto connector = find_apollo_monitor_connector_once();
	          if (!connector.empty()) {
	            return connector;
	          }

	          std::this_thread::sleep_for(100ms);
	        }

	        return {};
	      }

	      std::string find_apollo_monitor_connector_once() {
	        GError *raw_error = nullptr;
	        auto state = g_dbus_connection_call_sync(
	          bus,
	          "org.gnome.Mutter.DisplayConfig",
	          "/org/gnome/Mutter/DisplayConfig",
	          "org.gnome.Mutter.DisplayConfig",
	          "GetCurrentState",
	          nullptr,
	          nullptr,
	          G_DBUS_CALL_FLAGS_NONE,
	          2000,
	          nullptr,
	          &raw_error
	        );

	        if (!state) {
	          gerror_ptr dbus_error(raw_error);
	          BOOST_LOG(debug) << "Unable to query Mutter DisplayConfig for PipeWire monitor capture: "sv
	                           << (dbus_error ? dbus_error->message : "unknown");
	          return {};
	        }

	        auto monitors = g_variant_get_child_value(state, 1);
	        if (!monitors) {
	          g_variant_unref(state);
	          return {};
	        }

	        std::string best_exact_connector;
	        std::string best_apollo_connector;
	        const auto monitor_count = g_variant_n_children(monitors);
	        for (gsize index = 0; index < monitor_count; ++index) {
	          auto monitor = g_variant_get_child_value(monitors, index);
	          if (!monitor) {
	            continue;
	          }

	          auto spec = g_variant_get_child_value(monitor, 0);
	          auto properties = g_variant_get_child_value(monitor, 2);
	          if (!spec || !properties) {
	            if (spec) {
	              g_variant_unref(spec);
	            }
	            if (properties) {
	              g_variant_unref(properties);
	            }
	            g_variant_unref(monitor);
	            continue;
	          }

	          auto connector = variant_child_string(spec, 0).value_or(std::string {});
	          auto vendor = variant_child_string(spec, 1).value_or(std::string {});
	          auto product = variant_child_string(spec, 2).value_or(std::string {});
	          auto mutter_display_name = lookup_string_property(properties, "display-name");

	          if (is_apollo_monitor_spec(connector, vendor, product, mutter_display_name)) {
	            BOOST_LOG(info) << "Found Apollo Mutter monitor candidate: connector="sv << connector
	                            << " vendor="sv << vendor
	                            << " product="sv << product
	                            << " display-name="sv << mutter_display_name;

	            if (monitor_has_requested_mode(monitor)) {
	              best_exact_connector = connector;
	            } else if (best_apollo_connector.empty()) {
	              best_apollo_connector = connector;
	            }
	          }

	          g_variant_unref(properties);
	          g_variant_unref(spec);
	          g_variant_unref(monitor);
	        }

	        g_variant_unref(monitors);
	        g_variant_unref(state);

	        if (!best_exact_connector.empty()) {
	          return best_exact_connector;
	        }

	        return best_apollo_connector;
	      }

	      bool monitor_has_requested_mode(GVariant *monitor) {
	        auto modes = g_variant_get_child_value(monitor, 1);
	        if (!modes) {
	          return false;
	        }

	        bool found = false;
	        const auto mode_count = g_variant_n_children(modes);
	        for (gsize index = 0; index < mode_count; ++index) {
	          auto mode = g_variant_get_child_value(modes, index);
	          if (!mode) {
	            continue;
	          }

	          auto width_value = g_variant_get_child_value(mode, 1);
	          auto height_value = g_variant_get_child_value(mode, 2);
	          if (width_value && height_value) {
	            auto mode_width = variant_int(width_value);
	            auto mode_height = variant_int(height_value);
	            found = mode_width == width && mode_height == height;
	          }

	          if (width_value) {
	            g_variant_unref(width_value);
	          }
	          if (height_value) {
	            g_variant_unref(height_value);
	          }
	          g_variant_unref(mode);

	          if (found) {
	            break;
	          }
	        }

	        g_variant_unref(modes);
	        return found;
	      }

	      std::int64_t variant_int(GVariant *value) {
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

	      bool call_no_args(const std::string &path, const char *interface, const char *method) {
        GError *raw_error = nullptr;
        auto result = g_dbus_connection_call_sync(
          bus,
          MUTTER_SCREENCAST,
          path.c_str(),
          interface,
          method,
          nullptr,
          nullptr,
          G_DBUS_CALL_FLAGS_NONE,
          MUTTER_DBUS_TIMEOUT_MS,
          nullptr,
          &raw_error
        );
        if (!result) {
          gerror_ptr dbus_error(raw_error);
          if (dbus_error && dbus_error->message && std::strstr(dbus_error->message, "Object does not exist")) {
            BOOST_LOG(debug) << "GNOME ScreenCast " << method << " skipped because the object already disappeared.";
            return true;
          }
          BOOST_LOG(error) << "GNOME ScreenCast " << method << " failed: "sv << (dbus_error ? dbus_error->message : "unknown");
          return false;
        }

        g_variant_unref(result);
        return true;
      }

      bool start_pipewire_stream() {
        pw_init(nullptr, nullptr);

        loop = pw_thread_loop_new("apollo-pipewire-capture", nullptr);
        if (!loop) {
          BOOST_LOG(error) << "Unable to create PipeWire thread loop."sv;
          return false;
        }

        pw_thread_loop_lock(loop);

        context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
        if (!context) {
          pw_thread_loop_unlock(loop);
          BOOST_LOG(error) << "Unable to create PipeWire context."sv;
          return false;
        }

        core = pw_context_connect(context, nullptr, 0);
        if (!core) {
          pw_thread_loop_unlock(loop);
          BOOST_LOG(error) << "Unable to connect PipeWire context."sv;
          return false;
        }

        static pw_stream_events events {};
        events.version = PW_VERSION_STREAM_EVENTS;
        events.state_changed = on_stream_state_changed;
        events.param_changed = on_stream_param_changed;
        events.process = on_stream_process;

        stream = pw_stream_new_simple(
          pw_thread_loop_get_loop(loop),
          "apollo-gnome-screencast",
          pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr
          ),
          &events,
          this
        );
        if (!stream) {
          pw_thread_loop_unlock(loop);
          BOOST_LOG(error) << "Unable to create PipeWire stream."sv;
          return false;
        }

        auto flags = static_cast<pw_stream_flags>(
          PW_STREAM_FLAG_AUTOCONNECT |
          PW_STREAM_FLAG_MAP_BUFFERS |
          PW_STREAM_FLAG_RT_PROCESS
        );

        std::uint8_t params_buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
        const spa_rectangle requested_size {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        const spa_rectangle min_size {1, 1};
        const spa_fraction no_fixed_rate {0, 1};
        const spa_fraction requested_rate {framerate, 1};
        const spa_fraction min_rate {1, 1};

        const spa_pod *params[] {
          static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder,
            SPA_TYPE_OBJECT_Format,
            SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&requested_size, &min_size, &requested_size),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&no_fixed_rate),
            SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&requested_rate, &min_rate, &requested_rate)
          ))
        };

        if (pw_stream_connect(stream, PW_DIRECTION_INPUT, node_id, flags, params, 1) < 0) {
          pw_thread_loop_unlock(loop);
          BOOST_LOG(error) << "Unable to connect PipeWire stream to GNOME node " << node_id;
          return false;
        }

        if (pw_thread_loop_start(loop) < 0) {
          pw_thread_loop_unlock(loop);
          BOOST_LOG(error) << "Unable to start PipeWire thread loop."sv;
          return false;
        }

	        pw_thread_loop_unlock(loop);
	        running = true;
	        process_diag_at = std::chrono::steady_clock::now();
	        return true;
	      }

	      void log_process_diag(std::size_t bytes, double copy_ms, std::uint32_t data_type, std::int32_t stride) {
	        ++process_frames;
	        process_bytes += bytes;
	        process_max_copy_ms = std::max(process_max_copy_ms, copy_ms);

	        const auto now = std::chrono::steady_clock::now();
	        if (now - process_diag_at < 1s) {
	          return;
	        }

	        const auto elapsed = std::chrono::duration<double>(now - process_diag_at).count();
	        const auto mbps = elapsed > 0 ? (static_cast<double>(process_bytes) / (1024.0 * 1024.0)) / elapsed : 0.0;
	        BOOST_LOG(info) << "GNOME PipeWire process diag display=" << display_name
	                        << " frames=" << process_frames
	                        << " latest=" << latest_width << 'x' << latest_height
	                        << " data_type=" << data_type
	                        << " stride=" << stride
	                        << " mbps=" << mbps
	                        << " max_copy_ms=" << process_max_copy_ms;

	        process_diag_at = now;
	        process_frames = 0;
	        process_bytes = 0;
	        process_max_copy_ms = 0;
	      }

	      void log_capture_diag(bool copied, double wait_ms, double copy_ms) {
	        ++capture_frames;
	        if (copied) {
	          ++capture_new_frames;
	        } else {
	          ++capture_repeated_frames;
	        }
	        capture_max_wait_ms = std::max(capture_max_wait_ms, wait_ms);
	        capture_max_copy_ms = std::max(capture_max_copy_ms, copy_ms);

	        const auto now = std::chrono::steady_clock::now();
	        if (now - capture_diag_at < 1s) {
	          return;
	        }

	        BOOST_LOG(info) << "GNOME PipeWire capture diag display=" << display_name
	                        << " frames=" << capture_frames
	                        << " new=" << capture_new_frames
	                        << " repeated=" << capture_repeated_frames
	                        << " max_wait_ms=" << capture_max_wait_ms
	                        << " max_copy_ms=" << capture_max_copy_ms;

	        capture_diag_at = now;
	        capture_frames = 0;
	        capture_new_frames = 0;
	        capture_repeated_frames = 0;
	        capture_max_wait_ms = 0;
	        capture_max_copy_ms = 0;
	      }

	      void stop() {
        running = false;
        frame_cv.notify_all();
        node_cv.notify_all();

        if (owns_mutter_session && stream_path.size()) {
          call_no_args(stream_path, "org.gnome.Mutter.ScreenCast.Stream", "Stop");
        }
        if (owns_mutter_session && session_path.size()) {
          call_no_args(session_path, "org.gnome.Mutter.ScreenCast.Session", "Stop");
        }

        if (bus && signal_id) {
          g_dbus_connection_signal_unsubscribe(bus, signal_id);
          signal_id = 0;
        }

        if (loop) {
          pw_thread_loop_stop(loop);
          pw_thread_loop_lock(loop);
        }
        if (stream) {
          pw_stream_destroy(stream);
          stream = nullptr;
        }
        if (core) {
          pw_core_disconnect(core);
          core = nullptr;
        }
        if (context) {
          pw_context_destroy(context);
          context = nullptr;
        }
        if (loop) {
          pw_thread_loop_unlock(loop);
          pw_thread_loop_destroy(loop);
          loop = nullptr;
        }
        if (bus) {
          g_object_unref(bus);
          bus = nullptr;
        }
      }

      std::string display_name;
      mem_type_e mem_type {};
      std::uint32_t framerate {};
      std::chrono::nanoseconds frame_interval {16ms};
      std::atomic<bool> running {false};

      GDBusConnection *bus {};
      std::string session_path;
      std::string stream_path;
      bool owns_mutter_session {true};
      guint signal_id {};
      std::mutex node_mutex;
      std::condition_variable node_cv;
      guint node_id {};

      pw_thread_loop *loop {};
      pw_context *context {};
      pw_core *core {};
      pw_stream *stream {};

      std::mutex frame_mutex;
      std::condition_variable frame_cv;
	      std::vector<std::uint8_t> latest_pixels;
	      int latest_width {};
	      int latest_height {};
	      int latest_stride {};
	      std::uint64_t latest_generation {};
	      std::uint64_t consumed_generation {};
	      std::chrono::steady_clock::time_point latest_timestamp {};
	      spa_video_format pipewire_format {SPA_VIDEO_FORMAT_UNKNOWN};
	      std::chrono::steady_clock::time_point process_diag_at {};
	      std::uint64_t process_frames {};
	      std::uint64_t process_bytes {};
	      double process_max_copy_ms {};
	      std::chrono::steady_clock::time_point capture_diag_at {};
	      std::uint64_t capture_frames {};
	      std::uint64_t capture_new_frames {};
	      std::uint64_t capture_repeated_frames {};
	      double capture_max_wait_ms {};
	      double capture_max_copy_ms {};
	    };
  }  // namespace

  std::shared_ptr<display_t> pipewire_display(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
    if (hwdevice_type != mem_type_e::system && hwdevice_type != mem_type_e::vaapi && hwdevice_type != mem_type_e::cuda) {
      BOOST_LOG(debug) << "GNOME PipeWire capture cannot initialize with this memory type."sv;
      return nullptr;
    }

    auto display = std::make_shared<pipewire_display_t>();
    if (display->init(hwdevice_type, display_name, config)) {
      return nullptr;
    }

    return display;
  }

  std::vector<std::string> pipewire_display_names() {
    if (!mutter_screencast_available()) {
      return {};
    }

    return {"pipewire-virtual"};
  }
}  // namespace platf

#endif
