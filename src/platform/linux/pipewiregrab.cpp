/**
 * @file src/platform/linux/pipewiregrab.cpp
 * @brief GNOME Mutter ScreenCast/PipeWire capture.
 */

#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// lib includes
#include <drm_fourcc.h>
#include <gio/gio.h>
#include <pipewire/pipewire.h>
#include <spa/param/buffers.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <unistd.h>

// local includes
#include "graphics.h"
#include "cuda.h"
#include "mutter_dbus.h"
#include "src/config.h"
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/video.h"
#include "vaapi.h"
#include "virtual_display.h"

using namespace std::literals;

namespace platf {
  namespace {
	    std::string upper_copy(std::string value) {
	      std::transform(std::begin(value), std::end(value), std::begin(value), [](unsigned char c) {
	        return static_cast<char>(std::toupper(c));
	      });
	      return value;
	    }

	    bool is_apollo_monitor_spec(const std::string &connector, const std::string &vendor, const std::string &product, const std::string &display_name) {
	      auto haystack = upper_copy(connector + " " + vendor + " " + product + " " + display_name);
	      return upper_copy(vendor) == "APL" || haystack.find("APOLLO") != std::string::npos || haystack.find("VDISP") != std::string::npos;
	    }

	    bool mutter_screencast_available() {
      GError *raw_error = nullptr;
      auto bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &raw_error);
      mutter_dbus::gerror_ptr error(raw_error);
      if (!bus) {
        return false;
      }

      const bool has_owner = mutter_dbus::name_has_owner(
        bus,
        mutter_dbus::SCREENCAST_SERVICE,
        mutter_dbus::QUICK_CALL_TIMEOUT_MS
      );
      g_object_unref(bus);
	      return has_owner;
	    }

      enum class pipewire_capture_mode_e {
        MAPPED,
        DMABUF
      };

      const char *pipewire_capture_mode_name(pipewire_capture_mode_e mode) {
        return mode == pipewire_capture_mode_e::DMABUF ? "dmabuf" : "mapped";
      }

      std::optional<std::uint32_t> pipewire_format_to_drm_fourcc(spa_video_format format) {
        switch (format) {
          case SPA_VIDEO_FORMAT_BGRx:
            return DRM_FORMAT_XRGB8888;
          case SPA_VIDEO_FORMAT_BGRA:
            return DRM_FORMAT_ARGB8888;
          case SPA_VIDEO_FORMAT_RGBx:
            return DRM_FORMAT_XBGR8888;
          case SPA_VIDEO_FORMAT_RGBA:
            return DRM_FORMAT_ABGR8888;
          default:
            return std::nullopt;
        }
      }

      void close_surface_fds(egl::surface_descriptor_t &sd) {
        for (auto &fd : sd.fds) {
          if (fd >= 0) {
            close(fd);
            fd = -1;
          }
        }
      }

      struct dmabuf_frame_t {
        dmabuf_frame_t() {
          std::fill(std::begin(sd.fds), std::end(sd.fds), -1);
        }

        dmabuf_frame_t(const dmabuf_frame_t &) = delete;
        dmabuf_frame_t &operator=(const dmabuf_frame_t &) = delete;

        dmabuf_frame_t(dmabuf_frame_t &&other) noexcept {
          move_from(std::move(other));
        }

        dmabuf_frame_t &operator=(dmabuf_frame_t &&other) noexcept {
          if (this != &other) {
            reset();
            move_from(std::move(other));
          }
          return *this;
        }

        ~dmabuf_frame_t() {
          reset();
        }

        void reset() {
          close_surface_fds(sd);
          valid = false;
        }

        void move_to(egl::img_descriptor_t &img) {
          img.reset();
          img.sd = sd;
          std::fill(std::begin(sd.fds), std::end(sd.fds), -1);
          img.frame_timestamp = timestamp;
          img.data = nullptr;
          valid = false;
        }

        void move_from(dmabuf_frame_t &&other) {
          sd = other.sd;
          timestamp = other.timestamp;
          valid = other.valid;
          std::fill(std::begin(other.sd.fds), std::end(other.sd.fds), -1);
          other.valid = false;
        }

        egl::surface_descriptor_t sd {};
        std::chrono::steady_clock::time_point timestamp {};
        bool valid {};
      };

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
	        configure_dmabuf_policy();
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
	        if (active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	          return capture_dmabuf(push_captured_image_cb, pull_free_image_cb);
	        }

	        return capture_mapped(push_captured_image_cb, pull_free_image_cb);
	      }

	      capture_e capture_mapped(const push_captured_image_cb_t &push_captured_image_cb, const pull_free_image_cb_t &pull_free_image_cb) {
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

	      capture_e capture_dmabuf(const push_captured_image_cb_t &push_captured_image_cb, const pull_free_image_cb_t &pull_free_image_cb) {
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

	          bool captured = false;
	          double wait_ms = 0;
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

	            if (latest_dmabuf.valid && latest_generation != consumed_generation) {
	              auto descriptor = static_cast<egl::img_descriptor_t *>(img.get());
	              descriptor->width = width;
	              descriptor->height = height;
	              descriptor->pixel_pitch = 4;
	              descriptor->row_pitch = width * 4;
	              descriptor->sequence = latest_generation;
	              descriptor->serial = std::numeric_limits<decltype(descriptor->serial)>::max();
	              latest_dmabuf.move_to(*descriptor);
	              consumed_generation = latest_generation;
	              captured = true;
	            }
	          }

	          log_capture_diag(captured, wait_ms, 0);

	          if (!push_captured_image_cb(std::move(img), captured)) {
	            return capture_e::ok;
	          }
	        }

	        return capture_e::interrupted;
	      }

	      std::shared_ptr<img_t> alloc_img() override {
	        if (active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	          auto img = std::make_shared<egl::img_descriptor_t>();
	          img->width = width;
	          img->height = height;
	          img->pixel_pitch = 4;
	          img->row_pitch = width * 4;
	          img->sequence = 0;
	          img->serial = std::numeric_limits<decltype(img->serial)>::max();
	          img->data = nullptr;
	          std::fill_n(img->sd.fds, 4, -1);
	          return img;
	        }

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
	        if (active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	          auto descriptor = static_cast<egl::img_descriptor_t *>(img);
	          descriptor->reset();
	          descriptor->sequence = 0;
	          descriptor->data = nullptr;
	          descriptor->width = width;
	          descriptor->height = height;
	          descriptor->pixel_pitch = 4;
	          descriptor->row_pitch = width * 4;
	          return 0;
	        }

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
	          if (active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	            return va::make_avcodec_encode_device(width, height, 0, 0, true);
	          }
	          return va::make_avcodec_encode_device(width, height, false);
	        }
#endif

#ifdef SUNSHINE_BUILD_CUDA
	        if (mem_type == mem_type_e::cuda) {
	          if (active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	            return cuda::make_avcodec_gl_encode_device(width, height, 0, 0);
	          }
	          return cuda::make_avcodec_encode_device(width, height, false);
	        }
#endif

        return std::make_unique<avcodec_encode_device_t>();
      }

	    private:
	      void configure_dmabuf_policy() {
	        const char *environment_override = std::getenv("APOLLO_PIPEWIRE_DMABUF");
	        if (environment_override && *environment_override && !VDISPLAY::parseLinuxPipeWireDmaBuf(environment_override)) {
	          BOOST_LOG(warning) << "Unknown APOLLO_PIPEWIRE_DMABUF="sv << environment_override << "; ignoring environment override.";
	        }
	        if (!config::video.linux_pipewire_dmabuf.empty() && !VDISPLAY::parseLinuxPipeWireDmaBuf(config::video.linux_pipewire_dmabuf)) {
	          BOOST_LOG(warning) << "Unknown linux_pipewire_dmabuf="sv << config::video.linux_pipewire_dmabuf << "; defaulting to auto.";
	        }

	        dmabuf_policy = VDISPLAY::resolveLinuxPipeWireDmaBuf(config::video.linux_pipewire_dmabuf, environment_override);
	        const bool encoder_can_import_dmabuf = mem_type == mem_type_e::cuda || mem_type == mem_type_e::vaapi;
	        dmabuf_allowed = dmabuf_policy != VDISPLAY::PIPEWIRE_DMABUF::OFF && encoder_can_import_dmabuf;
	        if (dmabuf_policy == VDISPLAY::PIPEWIRE_DMABUF::FORCE && !encoder_can_import_dmabuf) {
	          dmabuf_policy_error = true;
	          BOOST_LOG(error) << "PipeWire DMA-BUF capture was forced, but the selected encoder path cannot import DMA-BUF frames.";
	        } else if (dmabuf_policy != VDISPLAY::PIPEWIRE_DMABUF::OFF && !encoder_can_import_dmabuf) {
	          BOOST_LOG(info) << "PipeWire DMA-BUF capture requires CUDA or VAAPI import; using mapped PipeWire frames for this encoder.";
	        }

	        BOOST_LOG(info) << "GNOME PipeWire DMA-BUF policy is "sv << VDISPLAY::linuxPipeWireDmaBufName(dmabuf_policy)
	                        << "; capture import is " << (dmabuf_allowed ? "eligible" : "mapped");
	      }

	      void update_buffer_data_type(bool use_dmabuf) {
	        std::uint8_t params_buffer[256];
	        spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
	        const auto data_types = use_dmabuf ?
	                                  (1 << SPA_DATA_DmaBuf) :
	                                  ((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr));
	        const auto blocks = use_dmabuf ? 1 : 1;
	        const spa_pod *params[] {
	          static_cast<const spa_pod *>(spa_pod_builder_add_object(
	            &builder,
	            SPA_TYPE_OBJECT_ParamBuffers,
	            SPA_PARAM_Buffers,
	            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(blocks),
	            SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(data_types)
	          ))
	        };

	        if (pw_stream_update_params(stream, params, 1) < 0) {
	          BOOST_LOG(warning) << "Unable to update PipeWire buffer data type to "
	                             << (use_dmabuf ? "DMA-BUF" : "mapped memory") << '.';
	        }
	      }

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

	        const bool negotiated_dmabuf = (video_info.flags & SPA_VIDEO_FLAG_MODIFIER) != 0;
	        const bool use_dmabuf = self->dmabuf_allowed && negotiated_dmabuf;

	        {
	          std::lock_guard lock(self->frame_mutex);
	          self->pipewire_format = video_info.format;
	          self->pipewire_modifier = negotiated_dmabuf ? video_info.modifier : DRM_FORMAT_MOD_INVALID;
	          self->active_capture_mode = use_dmabuf ? pipewire_capture_mode_e::DMABUF : pipewire_capture_mode_e::MAPPED;
	          if (video_info.size.width > 0 && video_info.size.height > 0) {
	            self->latest_width = static_cast<int>(video_info.size.width);
	            self->latest_height = static_cast<int>(video_info.size.height);
	            self->width = self->latest_width;
	            self->height = self->latest_height;
	            self->env_width = self->width;
	            self->env_height = self->height;
	          }

	          if (self->dmabuf_policy == VDISPLAY::PIPEWIRE_DMABUF::FORCE && !use_dmabuf) {
	            self->format_failed = true;
	          }
	          self->format_ready = true;
	        }

	        self->update_buffer_data_type(use_dmabuf);
	        self->format_cv.notify_all();

	        BOOST_LOG(info) << "GNOME PipeWire capture format "
	                        << self->latest_width << 'x' << self->latest_height
	                        << " spa_format=" << static_cast<int>(self->pipewire_format)
	                        << " modifier=" << self->pipewire_modifier
	                        << " capture_path=" << pipewire_capture_mode_name(self->active_capture_mode);
	      }

	      static void on_stream_process(void *data) {
		        auto self = static_cast<pipewire_display_t *>(data);
		        auto buffer = pw_stream_dequeue_buffer(self->stream);
	        if (!buffer) {
	          return;
	        }

	        auto spa_buffer = buffer->buffer;
	        if (!spa_buffer || spa_buffer->n_datas == 0 || !spa_buffer->datas[0].chunk) {
	          pw_stream_queue_buffer(self->stream, buffer);
	          return;
	        }

	        const auto &data0 = spa_buffer->datas[0];
	        const auto *chunk = data0.chunk;
	        if (data0.type == SPA_DATA_DmaBuf && self->active_capture_mode == pipewire_capture_mode_e::DMABUF) {
	          const auto import_start = std::chrono::steady_clock::now();
	          auto frame = self->make_dmabuf_frame(spa_buffer);
	          if (!frame) {
	            if (self->dmabuf_policy == VDISPLAY::PIPEWIRE_DMABUF::FORCE) {
	              BOOST_LOG(error) << "Forced PipeWire DMA-BUF capture received an unimportable buffer; stopping capture.";
	              self->running = false;
	              self->frame_cv.notify_all();
	            }
	            pw_stream_queue_buffer(self->stream, buffer);
	            return;
	          }

	          {
	            std::lock_guard lock(self->frame_mutex);
	            self->latest_dmabuf = std::move(*frame);
	            self->latest_timestamp = self->latest_dmabuf.timestamp;
	            ++self->latest_generation;
	          }
	          self->frame_cv.notify_all();
	          self->log_process_diag(
	            0,
	            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - import_start).count(),
	            data0.type,
	            chunk->stride
	          );

	          pw_stream_queue_buffer(self->stream, buffer);
	          return;
	        }

	        if (data0.type == SPA_DATA_DmaBuf && !data0.data) {
	          if (!self->logged_unexpected_unmapped_dmabuf) {
	            BOOST_LOG(warning) << "PipeWire delivered an unmapped DMA-BUF after Apollo selected mapped capture; dropping frames until PipeWire renegotiates.";
	            self->logged_unexpected_unmapped_dmabuf = true;
	          }
	          pw_stream_queue_buffer(self->stream, buffer);
	          return;
	        }

	        if (!data0.data) {
	          if (self->dmabuf_policy == VDISPLAY::PIPEWIRE_DMABUF::FORCE) {
	            BOOST_LOG(error) << "Forced PipeWire DMA-BUF capture received mapped buffer data_type=" << data0.type << "; stopping capture.";
	            self->running = false;
	            self->frame_cv.notify_all();
	          }
	          pw_stream_queue_buffer(self->stream, buffer);
	          return;
	        }

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

	      std::optional<dmabuf_frame_t> make_dmabuf_frame(spa_buffer *spa_buffer) {
	        auto fourcc = pipewire_format_to_drm_fourcc(pipewire_format);
	        if (!fourcc) {
	          BOOST_LOG(warning) << "PipeWire DMA-BUF format is not importable by Apollo: spa_format="
	                             << static_cast<int>(pipewire_format);
	          return std::nullopt;
	        }

	        dmabuf_frame_t frame;
	        frame.sd.width = latest_width > 0 ? latest_width : width;
	        frame.sd.height = latest_height > 0 ? latest_height : height;
	        frame.sd.fourcc = *fourcc;
	        frame.sd.modifier = pipewire_modifier;

	        const auto plane_count = std::min<std::uint32_t>(spa_buffer->n_datas, 4);
	        for (std::uint32_t plane = 0; plane < plane_count; ++plane) {
	          const auto &data = spa_buffer->datas[plane];
	          if (data.type != SPA_DATA_DmaBuf || data.fd < 0 || !data.chunk) {
	            continue;
	          }

	          auto fd = dup(static_cast<int>(data.fd));
	          if (fd < 0) {
	            BOOST_LOG(warning) << "Unable to duplicate PipeWire DMA-BUF fd for plane " << plane << ": " << strerror(errno);
	            return std::nullopt;
	          }

	          frame.sd.fds[plane] = fd;
	          frame.sd.pitches[plane] = data.chunk->stride > 0 ? static_cast<std::uint32_t>(data.chunk->stride) : static_cast<std::uint32_t>(width * 4);
	          frame.sd.offsets[plane] = data.chunk->offset;
	        }

	        if (frame.sd.fds[0] < 0) {
	          BOOST_LOG(warning) << "PipeWire DMA-BUF buffer did not include an importable first plane.";
	          return std::nullopt;
	        }

	        frame.timestamp = std::chrono::steady_clock::now();
	        frame.valid = true;
	        return frame;
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
          mutter_dbus::gerror_ptr dbus_error(raw_error);
          BOOST_LOG(error) << "Unable to connect to session bus for GNOME PipeWire capture: "sv << (dbus_error ? dbus_error->message : "unknown");
          return false;
        }

        GVariantBuilder session_props;
        g_variant_builder_init(&session_props, G_VARIANT_TYPE("a{sv}"));

        auto session_result = mutter_dbus::call_sync(
          bus,
          mutter_dbus::SCREENCAST_SERVICE,
          mutter_dbus::SCREENCAST_PATH,
          "org.gnome.Mutter.ScreenCast",
          "CreateSession",
          g_variant_new("(a{sv})", &session_props),
          G_VARIANT_TYPE("(o)"),
          mutter_dbus::STREAM_CALL_TIMEOUT_MS,
          &raw_error
        );
        if (!session_result) {
          mutter_dbus::gerror_ptr dbus_error(raw_error);
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
	        auto stream_result = mutter_dbus::call_sync(
	          bus,
	          mutter_dbus::SCREENCAST_SERVICE,
	          session_path.c_str(),
	          "org.gnome.Mutter.ScreenCast.Session",
	          record_existing_monitor ? "RecordMonitor" : "RecordVirtual",
	          record_existing_monitor ? g_variant_new("(sa{sv})", connector_name.c_str(), &props) : g_variant_new("(a{sv})", &props),
	          G_VARIANT_TYPE("(o)"),
	          mutter_dbus::STREAM_CALL_TIMEOUT_MS,
          &raw_error
        );
	        if (!stream_result) {
	          mutter_dbus::gerror_ptr dbus_error(raw_error);
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
          mutter_dbus::SCREENCAST_SERVICE,
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
	        auto state = mutter_dbus::call_sync(
	          bus,
	          mutter_dbus::DISPLAY_CONFIG_SERVICE,
	          mutter_dbus::DISPLAY_CONFIG_PATH,
	          "org.gnome.Mutter.DisplayConfig",
	          "GetCurrentState",
	          nullptr,
	          nullptr,
	          2000,
	          &raw_error
	        );

	        if (!state) {
	          mutter_dbus::gerror_ptr dbus_error(raw_error);
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

	          auto connector = mutter_dbus::child_string(spec, 0).value_or(std::string {});
	          auto vendor = mutter_dbus::child_string(spec, 1).value_or(std::string {});
	          auto product = mutter_dbus::child_string(spec, 2).value_or(std::string {});
	          auto mutter_display_name = mutter_dbus::string_property(properties, "display-name");

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
	            auto mode_width = mutter_dbus::int64_from_variant(width_value);
	            auto mode_height = mutter_dbus::int64_from_variant(height_value);
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

	      bool call_no_args(const std::string &path, const char *interface, const char *method) {
        return mutter_dbus::call_no_args(
          bus,
          mutter_dbus::SCREENCAST_SERVICE,
          path.c_str(),
          interface,
          method,
          mutter_dbus::STREAM_CALL_TIMEOUT_MS,
          "GNOME ScreenCast"
        );
      }

	      bool start_pipewire_stream() {
	        if (dmabuf_policy_error) {
	          return false;
	        }

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

	        std::uint8_t params_buffer[4096];
	        spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
	        const spa_rectangle requested_size {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
	        const spa_rectangle min_size {1, 1};
	        const spa_fraction no_fixed_rate {0, 1};
	        const spa_fraction requested_rate {framerate, 1};
	        const spa_fraction min_rate {1, 1};

	        std::array<const spa_pod *, 4> params {};
	        std::uint32_t n_params = 0;

	        if (dmabuf_allowed) {
	          params[n_params++] = build_pipewire_format(&builder, SPA_VIDEO_FORMAT_BGRx, true, requested_size, min_size, no_fixed_rate, requested_rate, min_rate);
	          params[n_params++] = build_pipewire_format(&builder, SPA_VIDEO_FORMAT_BGRA, true, requested_size, min_size, no_fixed_rate, requested_rate, min_rate);
	        }

	        if (dmabuf_policy != VDISPLAY::PIPEWIRE_DMABUF::FORCE) {
	          params[n_params++] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
	            &builder,
	            SPA_TYPE_OBJECT_Format,
	            SPA_PARAM_EnumFormat,
	            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
	            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
	            SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
	            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&requested_size, &min_size, &requested_size),
	            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&no_fixed_rate),
	            SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&requested_rate, &min_rate, &requested_rate)
	          ));
	        }

	        if (n_params == 0) {
	          BOOST_LOG(error) << "No PipeWire capture formats are available for the selected DMA-BUF policy.";
	          pw_thread_loop_unlock(loop);
	          return false;
	        }

	        if (pw_stream_connect(stream, PW_DIRECTION_INPUT, node_id, flags, params.data(), n_params) < 0) {
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

	        if (dmabuf_allowed) {
	          std::unique_lock lock(frame_mutex);
	          if (!format_cv.wait_for(lock, 2s, [&]() {
	                return format_ready || format_failed || !running;
	              })) {
	            if (dmabuf_policy == VDISPLAY::PIPEWIRE_DMABUF::FORCE) {
	              running = false;
	              BOOST_LOG(error) << "Timed out waiting for forced PipeWire DMA-BUF format negotiation.";
	              return false;
	            }
	            dmabuf_allowed = false;
	            active_capture_mode = pipewire_capture_mode_e::MAPPED;
	            BOOST_LOG(warning) << "Timed out waiting for PipeWire DMA-BUF negotiation; using mapped PipeWire capture.";
	          } else if (format_failed) {
	            running = false;
	            BOOST_LOG(error) << "PipeWire DMA-BUF capture was forced, but Mutter did not negotiate DMA-BUF.";
	            return false;
	          }
	        }

	        process_diag_at = std::chrono::steady_clock::now();
	        BOOST_LOG(info) << "GNOME PipeWire capture path selected: "sv << pipewire_capture_mode_name(active_capture_mode);
	        return true;
		      }

	      const spa_pod *build_pipewire_format(
	        spa_pod_builder *builder,
	        spa_video_format format,
	        bool dmabuf,
	        const spa_rectangle &requested_size,
	        const spa_rectangle &min_size,
	        const spa_fraction &no_fixed_rate,
	        const spa_fraction &requested_rate,
	        const spa_fraction &min_rate
	      ) {
	        spa_pod_frame frame {};
	        spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
	        spa_pod_builder_add(
	          builder,
	          SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
	          SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
	          SPA_FORMAT_VIDEO_format, SPA_POD_Id(format),
	          SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&requested_size, &min_size, &requested_size),
	          SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&no_fixed_rate),
	          SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&requested_rate, &min_rate, &requested_rate),
	          0
	        );

	        if (dmabuf) {
	          spa_pod_frame choice {};
	          spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
	          spa_pod_builder_push_choice(builder, &choice, SPA_CHOICE_Enum, 0);
	          spa_pod_builder_long(builder, DRM_FORMAT_MOD_LINEAR);
	          spa_pod_builder_long(builder, DRM_FORMAT_MOD_LINEAR);
	          spa_pod_builder_long(builder, DRM_FORMAT_MOD_INVALID);
	          spa_pod_builder_pop(builder, &choice);
	        }

	        return static_cast<const spa_pod *>(spa_pod_builder_pop(builder, &frame));
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
	      std::condition_variable format_cv;
	      std::condition_variable frame_cv;
		      std::vector<std::uint8_t> latest_pixels;
	      dmabuf_frame_t latest_dmabuf;
		      int latest_width {};
		      int latest_height {};
		      int latest_stride {};
		      std::uint64_t latest_generation {};
		      std::uint64_t consumed_generation {};
		      std::chrono::steady_clock::time_point latest_timestamp {};
		      spa_video_format pipewire_format {SPA_VIDEO_FORMAT_UNKNOWN};
	      std::uint64_t pipewire_modifier {DRM_FORMAT_MOD_INVALID};
	      VDISPLAY::PIPEWIRE_DMABUF dmabuf_policy {VDISPLAY::PIPEWIRE_DMABUF::AUTO};
	      pipewire_capture_mode_e active_capture_mode {pipewire_capture_mode_e::MAPPED};
	      bool dmabuf_allowed {};
	      bool dmabuf_policy_error {};
	      bool format_ready {};
	      bool format_failed {};
	      bool logged_unexpected_unmapped_dmabuf {};
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
