/**
 * @file src/platform/linux/virtual_display.cpp
 * @brief Virtual display implementation for Linux.
 *
 * The default Ubuntu Wayland backend uses EVDI to provide a real compositor
 * monitor and GNOME Mutter ScreenCast/PipeWire to capture it. Pure
 * RecordVirtual and direct EVDI/KMS capture remain available through the
 * linux_virtual_display_backend config key. APOLLO_LINUX_VIRTUAL_BACKEND is
 * kept as an explicit developer override for diagnostics and bisection.
 */

// standard includes
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

// platform includes
#include <drm/drm.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef SUNSHINE_BUILD_PIPEWIRE
  #include <gio/gio.h>
#endif

// local includes
#include "misc.h"
#include "mutter_dbus.h"
#include "src/config.h"
#include "src/logging.h"
#include "virtual_display.h"

using namespace std::literals;
namespace fs = std::filesystem;

namespace VDISPLAY {

  // ============================================================================
  // EVDI Types and Function Pointers (loaded dynamically)
  // ============================================================================

  // EVDI structures (matching evdi_lib.h)
  struct evdi_lib_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
  };

  struct evdi_device_context;
  typedef struct evdi_device_context *evdi_handle;

  enum evdi_device_status {
    EVDI_AVAILABLE,
    EVDI_UNRECOGNIZED,
    EVDI_NOT_PRESENT
  };

  struct evdi_mode {
    int width;
    int height;
    int refresh_rate;
    int bits_per_pixel;
    unsigned int pixel_format;
  };

  struct evdi_rect {
    int x1, y1, x2, y2;
  };

  struct evdi_buffer {
    int id;
    void *buffer;
    int width;
    int height;
    int stride;
    struct evdi_rect *rects;
    int rect_count;
  };

  enum evdi_grabpix_mode {
    EVDI_GRABPIX_MODE_RECTS = 0,
    EVDI_GRABPIX_MODE_DIRTY = 1
  };

  struct evdi_grabpix_ioctl_t {
    evdi_grabpix_mode mode;
    int32_t buf_width;
    int32_t buf_height;
    int32_t buf_byte_stride;
    unsigned char *buffer;
    int32_t num_rects;
    drm_clip_rect *rects;
  };

  static_assert(sizeof(evdi_grabpix_ioctl_t) == 40);

  static constexpr auto EVDI_GRABPIX_DRM_COMMAND = 0x02;
  static constexpr unsigned long EVDI_GRABPIX_IOCTL =
    DRM_IOWR(DRM_COMMAND_BASE + EVDI_GRABPIX_DRM_COMMAND, evdi_grabpix_ioctl_t);

  struct evdi_cursor_set {
    int32_t hot_x;
    int32_t hot_y;
    uint32_t width;
    uint32_t height;
    uint8_t enabled;
    uint32_t buffer_length;
    uint32_t *buffer;
    uint32_t pixel_format;
    uint32_t stride;
  };

  struct evdi_cursor_move {
    int32_t x;
    int32_t y;
  };

  struct evdi_ddcci_data {
    uint16_t address;
    uint16_t flags;
    uint32_t buffer_length;
    uint8_t *buffer;
  };

  struct evdi_event_context {
    void (*dpms_handler)(int dpms_mode, void *user_data);
    void (*mode_changed_handler)(struct evdi_mode mode, void *user_data);
    void (*update_ready_handler)(int buffer_to_be_updated, void *user_data);
    void (*crtc_state_handler)(int state, void *user_data);
    void (*cursor_set_handler)(struct evdi_cursor_set cursor_set, void *user_data);
    void (*cursor_move_handler)(struct evdi_cursor_move cursor_move, void *user_data);
    void (*ddcci_data_handler)(struct evdi_ddcci_data ddcci_data, void *user_data);
    void *user_data;
  };

  // EVDI function pointer types
  typedef evdi_device_status (*fn_evdi_check_device)(int device);
  typedef evdi_handle (*fn_evdi_open)(int device);
  typedef int (*fn_evdi_add_device)(void);
  typedef void (*fn_evdi_close)(evdi_handle handle);
  typedef void (*fn_evdi_connect)(evdi_handle handle, const unsigned char *edid,
                                   const unsigned int edid_length,
                                   const uint32_t sku_area_limit);
  typedef void (*fn_evdi_disconnect)(evdi_handle handle);
  typedef void (*fn_evdi_grab_pixels)(evdi_handle handle, struct evdi_rect *rects, int *num_rects);
  typedef void (*fn_evdi_register_buffer)(evdi_handle handle, struct evdi_buffer buffer);
  typedef void (*fn_evdi_unregister_buffer)(evdi_handle handle, int bufferId);
  typedef bool (*fn_evdi_request_update)(evdi_handle handle, int bufferId);
  typedef void (*fn_evdi_handle_events)(evdi_handle handle, struct evdi_event_context *evtctx);
  typedef int (*fn_evdi_get_event_ready)(evdi_handle handle);
  typedef void (*fn_evdi_get_lib_version)(struct evdi_lib_version *version);

  // EVDI function pointers (loaded at runtime)
  static struct {
    void *lib_handle;
    fn_evdi_check_device check_device;
    fn_evdi_open open;
    fn_evdi_add_device add_device;
    fn_evdi_close close;
    fn_evdi_connect connect;
    fn_evdi_disconnect disconnect;
    fn_evdi_grab_pixels grab_pixels;
    fn_evdi_register_buffer register_buffer;
    fn_evdi_unregister_buffer unregister_buffer;
    fn_evdi_request_update request_update;
    fn_evdi_handle_events handle_events;
    fn_evdi_get_event_ready get_event_ready;
    fn_evdi_get_lib_version get_lib_version;
    bool loaded;
  } evdi = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false};

  class evdi_painter_t {
    struct frame_t {
      std::vector<std::uint8_t> pixels;
      int width {};
      int height {};
      int stride {};
      std::uint64_t generation {};
      std::chrono::steady_clock::time_point timestamp {};
    };

    struct publish_stats_t {
      bool published {};
      bool cloned {};
      bool full_frame {};
      int rect_count {};
      std::uint64_t dirty_pixels {};
      double clone_ms {};
      double publish_ms {};
    };

    struct diag_window_t {
      std::chrono::steady_clock::time_point started {};
      std::uint64_t updates {};
      std::uint64_t published {};
      std::uint64_t empty_updates {};
      std::uint64_t cow_clones {};
      std::uint64_t full_frames {};
      std::uint64_t rects {};
      std::uint64_t dirty_pixels {};
      double max_grab_ms {};
      double max_publish_ms {};
      double max_clone_ms {};
      double max_total_ms {};
    };

  public:
    evdi_painter_t() = default;
    evdi_painter_t(const evdi_painter_t &) = delete;
    evdi_painter_t &operator=(const evdi_painter_t &) = delete;

    ~evdi_painter_t() {
      stop();
    }

    void start(evdi_handle handle, std::string display_name, bool publish_frames) {
      stop();

      if (!handle || !evdi.loaded) {
        return;
      }

      this->handle = handle;
      this->display_name = std::move(display_name);
      this->publish_frames = publish_frames;
      running = true;
      thread = std::thread([this]() {
        run();
      });

      BOOST_LOG(info) << "[VDISPLAY] Started EVDI update pump for " << this->display_name
                      << (this->publish_frames ? " with frame publishing." : " as a PipeWire frame acknowledger.");
    }

    void stop() {
      running = false;
      frame_cv.notify_all();
      if (thread.joinable()) {
        thread.join();
      }

      clear_buffers();
      handle = nullptr;
      display_name.clear();
    }

    bool copy_latest_frame(
      std::uint8_t *dst,
      int width,
      int height,
      int dst_stride,
      std::chrono::milliseconds timeout,
      std::uint64_t &last_generation,
      std::chrono::steady_clock::time_point &frame_timestamp,
      bool require_new_frame
    ) {
      if (!publish_frames) {
        return false;
      }
      if (!dst || width <= 0 || height <= 0 || dst_stride <= 0) {
        return false;
      }

      std::shared_ptr<const frame_t> frame;
      std::unique_lock lock(frame_mutex);
      const auto wanted_generation = last_generation;
      auto frame_ready = [&]() {
        return !running || (latest_frame && (!require_new_frame || latest_frame->generation != wanted_generation));
      };

      if (!frame_ready()) {
        if (timeout.count() <= 0 || !frame_cv.wait_for(lock, timeout, frame_ready)) {
          return false;
        }
      }

      frame = latest_frame;
      lock.unlock();

      if (!frame ||
          (require_new_frame && frame->generation == wanted_generation) ||
          frame->width != width ||
          frame->height != height ||
          frame->stride < width * 4) {
        return false;
      }

      const auto copy_start = std::chrono::steady_clock::now();
      for (int y = 0; y < height; ++y) {
        auto src_row = frame->pixels.data() + (static_cast<std::size_t>(y) * frame->stride);
        auto dst_row = dst + (static_cast<std::size_t>(y) * dst_stride);
        std::memcpy(dst_row, src_row, static_cast<std::size_t>(width) * 4);
      }
      const auto copy_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copy_start).count();

      if (copy_ms >= 8.0) {
        BOOST_LOG(info) << "[VDISPLAY] EVDI capture copy diag display=" << display_name
                        << " generation=" << frame->generation
                        << " copy_ms=" << copy_ms
                        << " bytes=" << (static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4);
      }

      last_generation = frame->generation;
      frame_timestamp = frame->timestamp;
      return true;
    }

    bool grab_busy_for(std::chrono::milliseconds minimum_duration) {
      std::lock_guard lock(grab_state_mutex);
      return grab_in_progress && std::chrono::steady_clock::now() - grab_started >= minimum_duration;
    }

  private:
    struct buffer_t {
      int id {};
      int width {};
      int height {};
      int stride {};
      std::vector<uint8_t> pixels;
      std::array<evdi_rect, 16> rects {};
      bool registered {};
    };

    static void dpms_handler(int dpms_mode, void *user_data) {
      auto self = static_cast<evdi_painter_t *>(user_data);
      if (self) {
        BOOST_LOG(debug) << "[VDISPLAY] EVDI DPMS event for " << self->display_name << ": " << dpms_mode;
      }
    }

    static void mode_changed_handler(evdi_mode mode, void *user_data) {
      auto self = static_cast<evdi_painter_t *>(user_data);
      if (!self) {
        return;
      }

      self->handle_mode_changed(mode);
    }

    static void update_ready_handler(int buffer_to_be_updated, void *user_data) {
      auto self = static_cast<evdi_painter_t *>(user_data);
      if (!self) {
        return;
      }

      self->grab_pixels(buffer_to_be_updated);
    }

    static void cursor_set_handler(evdi_cursor_set cursor_set, void *) {
      free(cursor_set.buffer);
    }

    static void cursor_move_handler(evdi_cursor_move, void *) {
    }

    void run() {
      evdi_event_context event_context {};
      event_context.dpms_handler = &evdi_painter_t::dpms_handler;
      event_context.mode_changed_handler = &evdi_painter_t::mode_changed_handler;
      event_context.update_ready_handler = &evdi_painter_t::update_ready_handler;
      event_context.cursor_set_handler = &evdi_painter_t::cursor_set_handler;
      event_context.cursor_move_handler = &evdi_painter_t::cursor_move_handler;
      event_context.user_data = this;

      const int update_event_fd = evdi.get_event_ready(handle);
      if (update_event_fd < 0) {
        BOOST_LOG(warning) << "[VDISPLAY] EVDI update pump has no event fd for " << display_name;
        return;
      }

      event_fd = update_event_fd;
      while (running) {
        request_update();

        pollfd pfd {};
        pfd.fd = update_event_fd;
        pfd.events = POLLIN | POLLPRI;

        const int poll_status = poll(&pfd, 1, 100);
        if (!running) {
          break;
        }

        if (poll_status > 0 && (pfd.revents & (POLLIN | POLLPRI | POLLERR | POLLHUP))) {
          evdi.handle_events(handle, &event_context);
        } else if (poll_status < 0 && errno != EINTR) {
          BOOST_LOG(debug) << "[VDISPLAY] EVDI update pump poll failed for " << display_name << ": " << strerror(errno);
        }
      }
      event_fd = -1;
    }

    void handle_mode_changed(const evdi_mode &mode) {
      if (mode.width <= 0 || mode.height <= 0 || mode.bits_per_pixel <= 0) {
        BOOST_LOG(warning) << "[VDISPLAY] Ignoring invalid EVDI mode for " << display_name
                           << ": " << mode.width << "x" << mode.height
                           << "@" << mode.refresh_rate << " bpp=" << mode.bits_per_pixel;
        return;
      }

      clear_buffers();
      current_mode = mode;
      requested_buffer = -1;
      next_buffer = 0;

      const int bytes_per_pixel = std::max(1, mode.bits_per_pixel / 8);
      int stride_pixels = mode.width + 63;
      stride_pixels &= ~63;
      const int stride = stride_pixels * bytes_per_pixel;
      const auto buffer_size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(mode.height);

      buffers.reserve(2);
      for (int i = 0; i < 2; ++i) {
        auto &buffer = buffers.emplace_back();
        buffer.id = next_buffer_id++;
        buffer.width = mode.width;
        buffer.height = mode.height;
        buffer.stride = stride;
        buffer.registered = false;

        if (publish_frames) {
          buffer.pixels.assign(buffer_size, 0);
          buffer.registered = true;

          evdi_buffer evdi_buffer {};
          evdi_buffer.id = buffer.id;
          evdi_buffer.buffer = buffer.pixels.data();
          evdi_buffer.width = buffer.width;
          evdi_buffer.height = buffer.height;
          evdi_buffer.stride = buffer.stride;
          evdi_buffer.rects = buffer.rects.data();
          evdi_buffer.rect_count = static_cast<int>(buffer.rects.size());

          evdi.register_buffer(handle, evdi_buffer);
        }
      }

      if (publish_frames) {
        std::lock_guard lock(frame_mutex);
        write_frame = std::make_shared<frame_t>();
        write_frame->width = mode.width;
        write_frame->height = mode.height;
        write_frame->stride = stride;
        write_frame->pixels.assign(buffer_size, 0);
        write_frame->generation = ++next_frame_generation;
        write_frame->timestamp = {};
        latest_frame.reset();
        frame_cv.notify_all();
      }

      BOOST_LOG(info) << "[VDISPLAY] EVDI update pump mode for " << display_name
                      << ": " << mode.width << "x" << mode.height
                      << "@" << mode.refresh_rate << "Hz, " << mode.bits_per_pixel << " bpp";

      request_update();
    }

    void request_update() {
      if (requested_buffer >= 0 || buffers.empty()) {
        return;
      }

      auto &buffer = buffers[next_buffer];
      requested_buffer = buffer.id;
      next_buffer = (next_buffer + 1) % buffers.size();

      if (evdi.request_update(handle, buffer.id)) {
        grab_pixels(buffer.id);
      }
    }

    void grab_pixels(int buffer_id) {
      if (requested_buffer != buffer_id) {
        return;
      }

      auto buffer = std::find_if(std::begin(buffers), std::end(buffers), [&](const buffer_t &candidate) {
        return candidate.id == buffer_id;
      });

      if (buffer == std::end(buffers)) {
        requested_buffer = -1;
        return;
      }

      const auto update_start = std::chrono::steady_clock::now();
      {
        std::lock_guard lock(grab_state_mutex);
        grab_started = update_start;
        grab_in_progress = true;
      }

      int rect_count = 0;
      if (publish_frames) {
        rect_count = static_cast<int>(buffer->rects.size());
        evdi.grab_pixels(handle, buffer->rects.data(), &rect_count);
      } else {
        acknowledge_update_without_frame_copy(rect_count);
      }
      const auto grab_done = std::chrono::steady_clock::now();
      {
        std::lock_guard lock(grab_state_mutex);
        grab_in_progress = false;
      }

      const auto stats = publish_frames ? publish_frame(*buffer, rect_count) : publish_stats_t {};
      const auto update_done = std::chrono::steady_clock::now();
      record_diag(
        rect_count,
        std::chrono::duration<double, std::milli>(grab_done - update_start).count(),
        std::chrono::duration<double, std::milli>(update_done - update_start).count(),
        stats
      );

      requested_buffer = -1;
      request_update();
    }

    bool acknowledge_update_without_frame_copy(int &rect_count) {
      rect_count = 0;

      if (event_fd < 0 || current_mode.width <= 0 || current_mode.height <= 0) {
        return false;
      }

      std::array<drm_clip_rect, 16> rects {};
      evdi_grabpix_ioctl_t cmd {};
      cmd.mode = EVDI_GRABPIX_MODE_DIRTY;
      cmd.buf_width = 1;
      cmd.buf_height = 1;
      cmd.buf_byte_stride = 4;
      cmd.buffer = nullptr;
      cmd.num_rects = static_cast<int>(rects.size());
      cmd.rects = rects.data();

      int status = -1;
      do {
        status = ioctl(event_fd, EVDI_GRABPIX_IOCTL, &cmd);
      } while (status < 0 && errno == EINTR);

      rect_count = std::max(0, cmd.num_rects);

      if (status == 0) {
        return true;
      }

      const int err = errno;
      if (err == EINVAL || err == EFAULT) {
        // Hybrid EVDI/PipeWire mode must keep EVDI's update/vblank loop moving,
        // but it must not publish pixels from the CPU-side EVDI buffer. EVDI has
        // no public "ack dirty rects without copying pixels" API. The DRM ioctl
        // currently clears dirty rects and emits vblank before rejecting this
        // deliberately undersized/null framebuffer, so PipeWire remains the sole
        // frame source while the compositor sees a live monitor.
        return true;
      }

      BOOST_LOG(debug) << "[VDISPLAY] EVDI update acknowledge ioctl failed for " << display_name
                       << ": " << strerror(err);
      return false;
    }

    publish_stats_t publish_frame(const buffer_t &buffer, int rect_count) {
      publish_stats_t stats {};
      const auto publish_start = std::chrono::steady_clock::now();

      if (buffer.width <= 0 || buffer.height <= 0 || buffer.stride <= 0 || buffer.pixels.empty()) {
        return stats;
      }

      std::shared_ptr<const frame_t> base_frame;
      {
        std::lock_guard lock(frame_mutex);
        if (write_frame && write_frame.use_count() > 2) {
          base_frame = latest_frame;
        }
      }

      std::shared_ptr<frame_t> copied_write_frame;
      if (base_frame) {
        const auto clone_start = std::chrono::steady_clock::now();
        copied_write_frame = std::make_shared<frame_t>(*base_frame);
        stats.cloned = true;
        stats.clone_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clone_start).count();
      }

      {
        std::lock_guard lock(frame_mutex);

        if (copied_write_frame) {
          write_frame = std::move(copied_write_frame);
        } else if (write_frame && latest_frame && write_frame.use_count() > 2) {
          const auto clone_start = std::chrono::steady_clock::now();
          write_frame = std::make_shared<frame_t>(*latest_frame);
          stats.cloned = true;
          stats.clone_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clone_start).count();
        }

        if (!write_frame ||
            write_frame->width != buffer.width ||
            write_frame->height != buffer.height ||
            write_frame->stride != buffer.stride ||
            write_frame->pixels.size() < buffer.pixels.size()) {
          write_frame = std::make_shared<frame_t>();
          write_frame->width = buffer.width;
          write_frame->height = buffer.height;
          write_frame->stride = buffer.stride;
          write_frame->pixels.assign(buffer.pixels.size(), 0);
          latest_frame.reset();
        }

        const int update_rect_count = std::clamp(rect_count, 0, static_cast<int>(buffer.rects.size()));
        stats.rect_count = update_rect_count;

        if (!latest_frame) {
          for (int y = 0; y < buffer.height; ++y) {
            auto src_row = buffer.pixels.data() + (static_cast<std::size_t>(y) * buffer.stride);
            auto dst_row = write_frame->pixels.data() + (static_cast<std::size_t>(y) * write_frame->stride);
            std::memcpy(dst_row, src_row, static_cast<std::size_t>(buffer.width) * 4);
          }
          stats.published = true;
          stats.full_frame = true;
          stats.dirty_pixels = static_cast<std::uint64_t>(buffer.width) * static_cast<std::uint64_t>(buffer.height);
        } else {
          for (int i = 0; i < update_rect_count; ++i) {
            const auto &rect = buffer.rects[i];
            const int x1 = std::clamp(std::min(rect.x1, rect.x2), 0, buffer.width);
            const int x2 = std::clamp(std::max(rect.x1, rect.x2), 0, buffer.width);
            const int y1 = std::clamp(std::min(rect.y1, rect.y2), 0, buffer.height);
            const int y2 = std::clamp(std::max(rect.y1, rect.y2), 0, buffer.height);

            if (x2 <= x1 || y2 <= y1) {
              continue;
            }

            const auto row_bytes = static_cast<std::size_t>(x2 - x1) * 4;
            for (int y = y1; y < y2; ++y) {
              auto src_row = buffer.pixels.data() + (static_cast<std::size_t>(y) * buffer.stride) + (x1 * 4);
              auto dst_row = write_frame->pixels.data() + (static_cast<std::size_t>(y) * write_frame->stride) + (x1 * 4);
              std::memcpy(dst_row, src_row, row_bytes);
            }
            stats.published = true;
            stats.dirty_pixels += static_cast<std::uint64_t>(x2 - x1) * static_cast<std::uint64_t>(y2 - y1);
          }
        }

        if (stats.published) {
          write_frame->timestamp = std::chrono::steady_clock::now();
          write_frame->generation = ++next_frame_generation;
          latest_frame = write_frame;
        }
      }

      if (stats.published) {
        frame_cv.notify_all();
      }

      stats.publish_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - publish_start).count();
      return stats;
    }

    void record_diag(int evdi_rect_count, double grab_ms, double total_ms, const publish_stats_t &stats) {
      const auto now = std::chrono::steady_clock::now();
      if (diag.started.time_since_epoch().count() == 0) {
        diag.started = now;
      }

      ++diag.updates;
      if (stats.published) {
        ++diag.published;
      } else {
        ++diag.empty_updates;
      }
      if (stats.cloned) {
        ++diag.cow_clones;
      }
      if (stats.full_frame) {
        ++diag.full_frames;
      }

      diag.rects += static_cast<std::uint64_t>(std::max(0, evdi_rect_count));
      diag.dirty_pixels += stats.dirty_pixels;
      diag.max_grab_ms = std::max(diag.max_grab_ms, grab_ms);
      diag.max_publish_ms = std::max(diag.max_publish_ms, stats.publish_ms);
      diag.max_clone_ms = std::max(diag.max_clone_ms, stats.clone_ms);
      diag.max_total_ms = std::max(diag.max_total_ms, total_ms);

      if (now - diag.started < 1s && total_ms < 25.0 && stats.clone_ms < 8.0) {
        return;
      }

      BOOST_LOG(info) << "[VDISPLAY] EVDI diag display=" << display_name
                      << " updates=" << diag.updates
                      << " published=" << diag.published
                      << " empty=" << diag.empty_updates
                      << " rects=" << diag.rects
                      << " dirty_pixels=" << diag.dirty_pixels
                      << " cow_clones=" << diag.cow_clones
                      << " full_frames=" << diag.full_frames
                      << " max_grab_ms=" << diag.max_grab_ms
                      << " max_publish_ms=" << diag.max_publish_ms
                      << " max_clone_ms=" << diag.max_clone_ms
                      << " max_total_ms=" << diag.max_total_ms;

      diag = {};
      diag.started = now;
    }

    void clear_buffers() {
      if (handle && evdi.loaded) {
        for (auto &buffer : buffers) {
          if (buffer.registered) {
            evdi.unregister_buffer(handle, buffer.id);
            buffer.registered = false;
          }
        }
      }

      buffers.clear();
      requested_buffer = -1;
      next_buffer = 0;
      current_mode = {};

      {
        std::lock_guard lock(frame_mutex);
        latest_frame.reset();
        write_frame.reset();
        ++next_frame_generation;
      }
      frame_cv.notify_all();
    }

    evdi_handle handle {};
    std::string display_name;
    bool publish_frames {true};
    std::atomic<bool> running {false};
    std::thread thread;
    evdi_mode current_mode {};
    int event_fd {-1};
    std::vector<buffer_t> buffers;
    int next_buffer_id {};
    std::size_t next_buffer {};
    int requested_buffer {-1};

    std::mutex frame_mutex;
    std::condition_variable frame_cv;
    std::shared_ptr<frame_t> write_frame;
    std::shared_ptr<const frame_t> latest_frame;
    std::uint64_t next_frame_generation {};
    diag_window_t diag;
    std::mutex grab_state_mutex;
    std::chrono::steady_clock::time_point grab_started {};
    bool grab_in_progress {};
  };

  // ============================================================================
  // Standard 1920x1080 EDID (used for virtual display)
  // ============================================================================

  // EDID for a generic 1920x1080@60Hz monitor
  static const unsigned char default_edid[] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,  // Header
    0x1E, 0x6D,  // Manufacturer ID (LG Display)
    0x00, 0x00,  // Product code
    0x01, 0x01, 0x01, 0x01,  // Serial number
    0x00, 0x1D,  // Week/Year of manufacture
    0x01, 0x04,  // EDID version 1.4
    0xB5,        // Video input (digital, 8-bit color depth, DisplayPort)
    0x3C, 0x22,  // Width/height in cm (60x34 = approx 27")
    0x78,        // Gamma 2.2
    0x3A,        // Features (RGB, preferred timing)
    // Chromaticity
    0xFC, 0x81, 0xA4, 0x55, 0x4D, 0x9D, 0x25, 0x12, 0x50, 0x54,
    // Established timings
    0x21, 0x08, 0x00,
    // Standard timings
    0xD1, 0xC0,  // 1920x1080@60Hz
    0x81, 0x80,  // 1280x1024@60Hz
    0x81, 0xC0,  // 1280x720@60Hz
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    // Detailed timing descriptor: 1920x1080@60Hz
    0x02, 0x3A,  // Pixel clock: 148.5 MHz
    0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
    0x58, 0x2C, 0x45, 0x00,
    0x56, 0x50, 0x21, 0x00, 0x00, 0x1E,
    // Display name descriptor
    0x00, 0x00, 0x00, 0xFC, 0x00,
    'A', 'P', 'O', 'L', 'L', 'O', ' ', 'V', 'D', 'I', 'S', 'P', '\n',
    // Display range limits
    0x00, 0x00, 0x00, 0xFD, 0x00,
    0x32, 0x4B, 0x1E, 0x51, 0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    // Extension flag and checksum (calculated)
    0x00, 0x00
  };

  // ============================================================================
  // Global State
  // ============================================================================

  static std::mutex vdisplay_mutex;
  static DRIVER_STATUS driver_status = DRIVER_STATUS::UNKNOWN;
  static std::atomic<bool> watchdog_running {false};
  static std::thread watchdog_thread;
  static bool evdi_available = false;
  static BACKEND selected_backend = BACKEND::UNKNOWN;

  bool should_run_evdi_painter(BACKEND backend) {
    if (std::getenv("APOLLO_EVDI_FORCE_PAINTER") != nullptr) {
      return true;
    }
    return std::getenv("APOLLO_EVDI_DISABLE_PAINTER") == nullptr;
  }

  bool should_publish_evdi_painter_frames(BACKEND backend) {
    if (backend == BACKEND::EVDI) {
      return true;
    }
    return std::getenv("APOLLO_EVDI_PUBLISH_PAINTER") != nullptr;
  }

  // Virtual display info structure
  struct VirtualDisplayInfo {
    std::string name;
    std::string guid_str;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    BACKEND backend;
    int device_index;      // EVDI device index
    evdi_handle handle;    // EVDI handle
    int drm_fd;            // DRM fd for card
    bool active;
    bool using_evdi;       // true if the backend owns a real EVDI monitor
    std::shared_ptr<evdi_painter_t> painter;
#ifdef SUNSHINE_BUILD_PIPEWIRE
    GDBusConnection *mutter_bus {};
    std::string mutter_remote_desktop_session_path;
    std::string mutter_remote_desktop_session_id;
    std::string mutter_session_path;
    std::string mutter_stream_path;
    uint32_t pipewire_node_id {};
#endif
  };

  static std::map<std::string, VirtualDisplayInfo> virtual_displays;

  static std::string lower_copy(std::string value) {
    std::transform(std::begin(value), std::end(value), std::begin(value), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return value;
  }

  static std::string normalized_backend_token(std::string_view value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
      return std::isspace(c);
    });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
      return std::isspace(c);
    }).base();
    if (begin >= end) {
      return {};
    }

    return lower_copy(std::string(begin, end));
  }

  std::optional<BACKEND> parseLinuxVirtualDisplayBackend(std::string_view value) {
    auto backend = normalized_backend_token(value);
    if (backend.empty()) {
      return std::nullopt;
    }

    if (backend == "evdi_pipewire" || backend == "evdi-pipewire" || backend == "hybrid" || backend == "auto") {
      return BACKEND::EVDI_PIPEWIRE;
    }
    if (backend == "mutter" || backend == "mutter_pipewire" || backend == "mutter-pipewire" || backend == "pipewire") {
      return BACKEND::MUTTER_PIPEWIRE;
    }
    if (backend == "evdi" || backend == "evdi_kms" || backend == "evdi-kms") {
      return BACKEND::EVDI;
    }

    return std::nullopt;
  }

  BACKEND resolveLinuxVirtualDisplayBackend(std::string_view config_value, const char *environment_override) {
    if (environment_override && *environment_override) {
      if (auto backend = parseLinuxVirtualDisplayBackend(environment_override)) {
        return *backend;
      }
    }

    if (auto backend = parseLinuxVirtualDisplayBackend(config_value)) {
      return *backend;
    }

    return BACKEND::EVDI_PIPEWIRE;
  }

  std::optional<CAPTURE_BACKEND> parseLinuxVirtualCaptureBackend(std::string_view value) {
    auto backend = normalized_backend_token(value);
    if (backend.empty()) {
      return std::nullopt;
    }

    if (backend == "auto") {
      return CAPTURE_BACKEND::AUTO;
    }
    if (backend == "pipewire" || backend == "mutter" || backend == "mutter_pipewire" || backend == "mutter-pipewire") {
      return CAPTURE_BACKEND::PIPEWIRE;
    }
    if (backend == "nvidia" || backend == "nvfbc" || backend == "nvidia_capture" || backend == "nvidia-capture") {
      return CAPTURE_BACKEND::NVIDIA;
    }

    return std::nullopt;
  }

  CAPTURE_BACKEND resolveLinuxVirtualCaptureBackend(std::string_view config_value, const char *environment_override) {
    if (environment_override && *environment_override) {
      if (auto backend = parseLinuxVirtualCaptureBackend(environment_override)) {
        return *backend;
      }
    }

    if (auto backend = parseLinuxVirtualCaptureBackend(config_value)) {
      return *backend;
    }

    return CAPTURE_BACKEND::AUTO;
  }

  const char *linuxVirtualCaptureBackendName(CAPTURE_BACKEND backend) {
    switch (backend) {
      case CAPTURE_BACKEND::AUTO:
        return "auto";
      case CAPTURE_BACKEND::PIPEWIRE:
        return "PipeWire";
      case CAPTURE_BACKEND::NVIDIA:
        return "NVIDIA";
      default:
        return "unknown";
    }
  }

  std::optional<PIPEWIRE_DMABUF> parseLinuxPipeWireDmaBuf(std::string_view value) {
    auto mode = normalized_backend_token(value);
    if (mode.empty()) {
      return std::nullopt;
    }

    if (mode == "auto") {
      return PIPEWIRE_DMABUF::AUTO;
    }
    if (mode == "off" || mode == "false" || mode == "disabled" || mode == "disable" || mode == "0") {
      return PIPEWIRE_DMABUF::OFF;
    }
    if (mode == "force" || mode == "forced" || mode == "on" || mode == "true" || mode == "enabled" || mode == "enable" || mode == "1") {
      return PIPEWIRE_DMABUF::FORCE;
    }

    return std::nullopt;
  }

  PIPEWIRE_DMABUF resolveLinuxPipeWireDmaBuf(std::string_view config_value, const char *environment_override) {
    if (environment_override && *environment_override) {
      if (auto mode = parseLinuxPipeWireDmaBuf(environment_override)) {
        return *mode;
      }
    }

    if (auto mode = parseLinuxPipeWireDmaBuf(config_value)) {
      return *mode;
    }

    return PIPEWIRE_DMABUF::AUTO;
  }

  const char *linuxPipeWireDmaBufName(PIPEWIRE_DMABUF mode) {
    switch (mode) {
      case PIPEWIRE_DMABUF::AUTO:
        return "auto";
      case PIPEWIRE_DMABUF::OFF:
        return "off";
      case PIPEWIRE_DMABUF::FORCE:
        return "force";
      default:
        return "unknown";
    }
  }

  const char *linuxVirtualDisplayBackendName(BACKEND backend) {
    switch (backend) {
      case BACKEND::MUTTER_PIPEWIRE:
        return "Mutter RecordVirtual/PipeWire";
      case BACKEND::EVDI_PIPEWIRE:
        return "EVDI monitor/PipeWire";
      case BACKEND::EVDI:
        return "EVDI/KMS";
      case BACKEND::UNKNOWN:
      default:
        return "unknown";
    }
  }

  static BACKEND configured_backend() {
    const char *environment_override = std::getenv("APOLLO_LINUX_VIRTUAL_BACKEND");
    if (environment_override && *environment_override) {
      if (parseLinuxVirtualDisplayBackend(environment_override)) {
        BOOST_LOG(info) << "[VDISPLAY] APOLLO_LINUX_VIRTUAL_BACKEND override is active.";
      } else {
        BOOST_LOG(warning) << "[VDISPLAY] Unknown APOLLO_LINUX_VIRTUAL_BACKEND=" << environment_override
                           << "; ignoring environment override.";
      }
    }

    if (!config::video.linux_virtual_display_backend.empty() &&
        !parseLinuxVirtualDisplayBackend(config::video.linux_virtual_display_backend)) {
      BOOST_LOG(warning) << "[VDISPLAY] Unknown linux_virtual_display_backend="
                         << config::video.linux_virtual_display_backend
                         << "; defaulting to EVDI monitor/PipeWire.";
    }

    return resolveLinuxVirtualDisplayBackend(config::video.linux_virtual_display_backend, environment_override);
  }

#ifdef SUNSHINE_BUILD_PIPEWIRE
  namespace mutter_dbus = platf::mutter_dbus;

  namespace {
    struct mutter_node_wait_t {
      std::mutex mutex;
      std::condition_variable cv;
      uint32_t node_id {};
    };
  }  // namespace

  static bool mutter_screencast_available() {
    if (!std::getenv("WAYLAND_DISPLAY")) {
      BOOST_LOG(error) << "[VDISPLAY] Mutter/PipeWire virtual display requires a Wayland session.";
      return false;
    }

    GError *raw_error = nullptr;
    auto bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &raw_error);
    mutter_dbus::gerror_ptr dbus_error(raw_error);
    if (!bus) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to connect to the session bus for Mutter/PipeWire virtual display: "
                       << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    if (!mutter_dbus::name_has_owner(bus, mutter_dbus::SCREENCAST_SERVICE, mutter_dbus::QUICK_CALL_TIMEOUT_MS)) {
      BOOST_LOG(error) << "[VDISPLAY] Mutter ScreenCast service is not available.";
      g_object_unref(bus);
      return false;
    }

    raw_error = nullptr;
    auto version_result = mutter_dbus::call_sync(
      bus,
      mutter_dbus::SCREENCAST_SERVICE,
      mutter_dbus::SCREENCAST_PATH,
      "org.freedesktop.DBus.Properties",
      "Get",
      g_variant_new("(ss)", "org.gnome.Mutter.ScreenCast", "Version"),
      G_VARIANT_TYPE("(v)"),
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    dbus_error.reset(raw_error);
    if (!version_result) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to query Mutter ScreenCast version: "
                       << (dbus_error ? dbus_error->message : "unknown");
      g_object_unref(bus);
      return false;
    }

    GVariant *version_value = nullptr;
    g_variant_get(version_result, "(v)", &version_value);
    const auto version = mutter_dbus::uint32_from_variant(version_value);
    if (version_value) {
      g_variant_unref(version_value);
    }
    g_variant_unref(version_result);
    g_object_unref(bus);

    if (version < 4) {
      BOOST_LOG(error) << "[VDISPLAY] Mutter ScreenCast version " << version
                       << " does not support the Apollo virtual display backend.";
      return false;
    }

    BOOST_LOG(info) << "[VDISPLAY] Mutter ScreenCast version " << version
                    << " is available for virtual displays.";
    return true;
  }

  static bool call_dbus_no_args(VirtualDisplayInfo &vdinfo, const char *destination, const std::string &path, const char *interface, const char *method) {
    if (!vdinfo.mutter_bus || path.empty()) {
      return true;
    }

    GError *raw_error = nullptr;
    auto result = mutter_dbus::call_sync(
      vdinfo.mutter_bus,
      destination,
      path.c_str(),
      interface,
      method,
      nullptr,
      nullptr,
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    mutter_dbus::gerror_ptr dbus_error(raw_error);
    if (!result) {
      if (mutter_dbus::error_is_missing_object(dbus_error.get())) {
        BOOST_LOG(debug) << "[VDISPLAY] Mutter DBus " << method
                         << " skipped because the object already disappeared.";
        return true;
      }
      BOOST_LOG(error) << "[VDISPLAY] Mutter DBus " << method
                       << " failed for " << vdinfo.name << ": "
                       << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    g_variant_unref(result);
    return true;
  }

  static void destroy_mutter_virtual_stream(VirtualDisplayInfo &vdinfo) {
    if (!vdinfo.mutter_bus) {
      return;
    }

    if (!vdinfo.mutter_remote_desktop_session_path.empty()) {
      call_dbus_no_args(
        vdinfo,
        mutter_dbus::REMOTE_DESKTOP_SERVICE,
        vdinfo.mutter_remote_desktop_session_path,
        "org.gnome.Mutter.RemoteDesktop.Session",
        "Stop"
      );
    } else {
      call_dbus_no_args(vdinfo, mutter_dbus::SCREENCAST_SERVICE, vdinfo.mutter_stream_path, "org.gnome.Mutter.ScreenCast.Stream", "Stop");
      call_dbus_no_args(vdinfo, mutter_dbus::SCREENCAST_SERVICE, vdinfo.mutter_session_path, "org.gnome.Mutter.ScreenCast.Session", "Stop");
    }

    g_object_unref(vdinfo.mutter_bus);
    vdinfo.mutter_bus = nullptr;
    vdinfo.mutter_remote_desktop_session_path.clear();
    vdinfo.mutter_remote_desktop_session_id.clear();
    vdinfo.mutter_stream_path.clear();
    vdinfo.mutter_session_path.clear();
    vdinfo.pipewire_node_id = 0;
  }

  static bool create_mutter_remote_desktop_session(VirtualDisplayInfo &vdinfo) {
    GError *raw_error = nullptr;
    auto session_result = mutter_dbus::call_sync(
      vdinfo.mutter_bus,
      mutter_dbus::REMOTE_DESKTOP_SERVICE,
      mutter_dbus::REMOTE_DESKTOP_PATH,
      "org.gnome.Mutter.RemoteDesktop",
      "CreateSession",
      nullptr,
      G_VARIANT_TYPE("(o)"),
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    mutter_dbus::gerror_ptr dbus_error(raw_error);
    if (!session_result) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to create Mutter RemoteDesktop session for " << vdinfo.name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    const char *session_path_raw = nullptr;
    g_variant_get(session_result, "(&o)", &session_path_raw);
    vdinfo.mutter_remote_desktop_session_path = session_path_raw ? session_path_raw : "";
    g_variant_unref(session_result);

    raw_error = nullptr;
    auto session_id_result = mutter_dbus::call_sync(
      vdinfo.mutter_bus,
      mutter_dbus::REMOTE_DESKTOP_SERVICE,
      vdinfo.mutter_remote_desktop_session_path.c_str(),
      "org.freedesktop.DBus.Properties",
      "Get",
      g_variant_new("(ss)", "org.gnome.Mutter.RemoteDesktop.Session", "SessionId"),
      G_VARIANT_TYPE("(v)"),
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    dbus_error.reset(raw_error);
    if (!session_id_result) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to read Mutter RemoteDesktop SessionId for " << vdinfo.name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    GVariant *session_id_value = nullptr;
    g_variant_get(session_id_result, "(v)", &session_id_value);
    if (session_id_value && g_variant_is_of_type(session_id_value, G_VARIANT_TYPE_STRING)) {
      vdinfo.mutter_remote_desktop_session_id = g_variant_get_string(session_id_value, nullptr);
    }
    if (session_id_value) {
      g_variant_unref(session_id_value);
    }
    g_variant_unref(session_id_result);

    if (vdinfo.mutter_remote_desktop_session_id.empty()) {
      BOOST_LOG(error) << "[VDISPLAY] Mutter RemoteDesktop session for " << vdinfo.name
                       << " did not expose a SessionId.";
      return false;
    }

    BOOST_LOG(info) << "[VDISPLAY] Created Mutter RemoteDesktop session for " << vdinfo.name
                    << " id=" << vdinfo.mutter_remote_desktop_session_id;
    return true;
  }

  static bool create_mutter_virtual_stream(VirtualDisplayInfo &vdinfo) {
    GError *raw_error = nullptr;
    vdinfo.mutter_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &raw_error);
    mutter_dbus::gerror_ptr dbus_error(raw_error);
    if (!vdinfo.mutter_bus) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to connect to the session bus for " << vdinfo.name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      return false;
    }

    if (!create_mutter_remote_desktop_session(vdinfo)) {
      destroy_mutter_virtual_stream(vdinfo);
      return false;
    }

    GVariantBuilder session_props;
    g_variant_builder_init(&session_props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(
      &session_props,
      "{sv}",
      "remote-desktop-session-id",
      g_variant_new_string(vdinfo.mutter_remote_desktop_session_id.c_str())
    );

    raw_error = nullptr;
    auto session_result = mutter_dbus::call_sync(
      vdinfo.mutter_bus,
      mutter_dbus::SCREENCAST_SERVICE,
      mutter_dbus::SCREENCAST_PATH,
      "org.gnome.Mutter.ScreenCast",
      "CreateSession",
      g_variant_new("(a{sv})", &session_props),
      G_VARIANT_TYPE("(o)"),
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    dbus_error.reset(raw_error);
    if (!session_result) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to create Mutter ScreenCast session for " << vdinfo.name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      destroy_mutter_virtual_stream(vdinfo);
      return false;
    }

    const char *session_path_raw = nullptr;
    g_variant_get(session_result, "(&o)", &session_path_raw);
    vdinfo.mutter_session_path = session_path_raw ? session_path_raw : "";
    g_variant_unref(session_result);

    GVariantBuilder props;
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&props, "{sv}", "cursor-mode", g_variant_new_uint32(1));
    g_variant_builder_add(&props, "{sv}", "is-recording", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&props, "{sv}", "is-platform", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&props, "{sv}", "width", g_variant_new_uint32(vdinfo.width));
    g_variant_builder_add(&props, "{sv}", "height", g_variant_new_uint32(vdinfo.height));
    g_variant_builder_add(&props, "{sv}", "framerate", g_variant_new_uint32(std::max<uint32_t>(1, vdinfo.fps / 1000)));

    raw_error = nullptr;
    auto stream_result = mutter_dbus::call_sync(
      vdinfo.mutter_bus,
      mutter_dbus::SCREENCAST_SERVICE,
      vdinfo.mutter_session_path.c_str(),
      "org.gnome.Mutter.ScreenCast.Session",
      "RecordVirtual",
      g_variant_new("(a{sv})", &props),
      G_VARIANT_TYPE("(o)"),
      mutter_dbus::QUICK_CALL_TIMEOUT_MS,
      &raw_error
    );
    dbus_error.reset(raw_error);
    if (!stream_result) {
      BOOST_LOG(error) << "[VDISPLAY] Unable to create Mutter virtual ScreenCast stream for " << vdinfo.name
                       << ": " << (dbus_error ? dbus_error->message : "unknown");
      destroy_mutter_virtual_stream(vdinfo);
      return false;
    }

    const char *stream_path_raw = nullptr;
    g_variant_get(stream_result, "(&o)", &stream_path_raw);
    vdinfo.mutter_stream_path = stream_path_raw ? stream_path_raw : "";
    g_variant_unref(stream_result);

    mutter_node_wait_t node_wait;
    const auto signal_id = g_dbus_connection_signal_subscribe(
      vdinfo.mutter_bus,
      mutter_dbus::SCREENCAST_SERVICE,
      "org.gnome.Mutter.ScreenCast.Stream",
      "PipeWireStreamAdded",
      vdinfo.mutter_stream_path.c_str(),
      nullptr,
      G_DBUS_SIGNAL_FLAGS_NONE,
      [](GDBusConnection *, const char *, const char *, const char *, const char *, GVariant *parameters, gpointer user_data) {
        auto state = static_cast<mutter_node_wait_t *>(user_data);
        guint node = 0;
        g_variant_get(parameters, "(u)", &node);
        {
          std::lock_guard lock(state->mutex);
          state->node_id = node;
        }
        state->cv.notify_all();
      },
      &node_wait,
      nullptr
    );

    const bool started = !vdinfo.mutter_remote_desktop_session_path.empty() ?
      call_dbus_no_args(
        vdinfo,
        mutter_dbus::REMOTE_DESKTOP_SERVICE,
        vdinfo.mutter_remote_desktop_session_path,
        "org.gnome.Mutter.RemoteDesktop.Session",
        "Start"
      ) :
      call_dbus_no_args(vdinfo, mutter_dbus::SCREENCAST_SERVICE, vdinfo.mutter_session_path, "org.gnome.Mutter.ScreenCast.Session", "Start");
    if (!started) {
      if (signal_id) {
        g_dbus_connection_signal_unsubscribe(vdinfo.mutter_bus, signal_id);
      }
      destroy_mutter_virtual_stream(vdinfo);
      return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
      while (g_main_context_iteration(nullptr, FALSE)) {
      }
      {
        std::lock_guard lock(node_wait.mutex);
        if (node_wait.node_id) {
          break;
        }
      }
      std::this_thread::sleep_for(20ms);
    }

    if (signal_id) {
      g_dbus_connection_signal_unsubscribe(vdinfo.mutter_bus, signal_id);
    }

    {
      std::lock_guard lock(node_wait.mutex);
      vdinfo.pipewire_node_id = node_wait.node_id;
    }

    if (!vdinfo.pipewire_node_id) {
      BOOST_LOG(error) << "[VDISPLAY] Timed out waiting for Mutter PipeWire node for " << vdinfo.name;
      destroy_mutter_virtual_stream(vdinfo);
      return false;
    }

    BOOST_LOG(info) << "[VDISPLAY] Created Mutter/PipeWire virtual display " << vdinfo.name
                    << " node=" << vdinfo.pipewire_node_id
                    << " mode=" << vdinfo.width << 'x' << vdinfo.height
                    << '@' << std::max<uint32_t>(1, vdinfo.fps / 1000) << "Hz";
    return true;
  }
#else
  static bool mutter_screencast_available() {
    BOOST_LOG(error) << "[VDISPLAY] Mutter/PipeWire virtual display backend is not compiled in.";
    return false;
  }
#endif

  // ============================================================================
  // EVDI Library Loading
  // ============================================================================

  static bool load_evdi_library() {
    if (evdi.loaded) {
      return true;
    }

    // Try to load libevdi.so
    const char *lib_names[] = {
      "libevdi.so.1",
      "libevdi.so",
      "/usr/lib/libevdi.so.1",
      "/usr/lib/libevdi.so",
      "/usr/local/lib/libevdi.so.1",
      "/usr/local/lib/libevdi.so"
    };

    for (const auto &lib_name : lib_names) {
      evdi.lib_handle = dlopen(lib_name, RTLD_NOW);
      if (evdi.lib_handle) {
        BOOST_LOG(info) << "[VDISPLAY] Loaded EVDI library: " << lib_name;
        break;
      }
    }

    if (!evdi.lib_handle) {
      BOOST_LOG(warning) << "[VDISPLAY] Could not load libevdi.so: " << dlerror();
      BOOST_LOG(warning) << "[VDISPLAY] Install 'evdi' package to use APOLLO_LINUX_VIRTUAL_BACKEND=evdi.";
      return false;
    }

    // Load function pointers
    #define LOAD_EVDI_FUNC(name) \
      evdi.name = (fn_evdi_##name)dlsym(evdi.lib_handle, "evdi_" #name); \
      if (!evdi.name) { \
        BOOST_LOG(error) << "[VDISPLAY] Failed to load evdi_" #name; \
        dlclose(evdi.lib_handle); \
        evdi.lib_handle = nullptr; \
        return false; \
      }

    LOAD_EVDI_FUNC(check_device);
    LOAD_EVDI_FUNC(open);
    LOAD_EVDI_FUNC(add_device);
    LOAD_EVDI_FUNC(close);
    LOAD_EVDI_FUNC(connect);
    LOAD_EVDI_FUNC(disconnect);
    LOAD_EVDI_FUNC(grab_pixels);
    LOAD_EVDI_FUNC(register_buffer);
    LOAD_EVDI_FUNC(unregister_buffer);
    LOAD_EVDI_FUNC(request_update);
    LOAD_EVDI_FUNC(handle_events);
    LOAD_EVDI_FUNC(get_event_ready);
    LOAD_EVDI_FUNC(get_lib_version);

    #undef LOAD_EVDI_FUNC

    // Check library version
    evdi_lib_version version;
    evdi.get_lib_version(&version);
    BOOST_LOG(info) << "[VDISPLAY] EVDI library version: "
                    << version.version_major << "."
                    << version.version_minor << "."
                    << version.version_patchlevel;

    evdi.loaded = true;
    return true;
  }

  static void unload_evdi_library() {
    if (evdi.lib_handle) {
      dlclose(evdi.lib_handle);
      evdi.lib_handle = nullptr;
    }
    evdi.loaded = false;
  }

  // ============================================================================
  // EVDI Module Check
  // ============================================================================

  static bool check_evdi_module_loaded() {
    // Check if evdi kernel module is loaded
    std::ifstream modules("/proc/modules");
    std::string line;
    while (std::getline(modules, line)) {
      if (line.find("evdi") != std::string::npos) {
        BOOST_LOG(info) << "[VDISPLAY] EVDI kernel module is loaded.";
        return true;
      }
    }

    // Also check sysfs
    if (fs::exists("/sys/module/evdi")) {
      BOOST_LOG(info) << "[VDISPLAY] EVDI kernel module detected via sysfs.";
      return true;
    }

    BOOST_LOG(warning) << "[VDISPLAY] EVDI kernel module is not loaded.";
    BOOST_LOG(warning) << "[VDISPLAY] Try: sudo modprobe evdi";
    return false;
  }

  // ============================================================================
  // Utility Functions
  // ============================================================================

  static std::string generate_display_name(const uuid_util::uuid_t &guid) {
    return "VIRTUAL-" + guid.string().substr(0, 8);
  }

  static int find_available_evdi_device() {
    // First pass: find an already-available EVDI device
    for (int i = 0; i < 16; i++) {
      auto status = evdi.check_device(i);
      if (status == EVDI_AVAILABLE) {
        return i;
      }
    }

    // No available device found; add a new one
    int result = evdi.add_device();
    if (result >= 0) {
      BOOST_LOG(info) << "[VDISPLAY] Added new EVDI device, scanning for index...";
      // Second pass: find the newly added device
      for (int i = 0; i < 16; i++) {
        auto status = evdi.check_device(i);
        if (status == EVDI_AVAILABLE) {
          BOOST_LOG(info) << "[VDISPLAY] Found new EVDI device at index: " << i;
          return i;
        }
      }
    }
    return -1;
  }

  static void calculate_edid_checksum(unsigned char *edid, size_t block_size = 128) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < block_size - 1; i++) {
      checksum += edid[i];
    }
    edid[block_size - 1] = 256 - checksum;
  }

  static void drop_drm_master_if_held(int fd, const std::string &context) {
    if (fd < 0) {
      return;
    }

    if (drmDropMaster(fd) == 0) {
      BOOST_LOG(debug) << "[VDISPLAY] Dropped DRM master for " << context;
      return;
    }

    if (errno != EINVAL && errno != EACCES && errno != EPERM) {
      BOOST_LOG(debug) << "[VDISPLAY] DRM master drop skipped for " << context << ": " << strerror(errno);
    }
  }

  static void probe_drm_connectors(int fd, const std::string &context) {
    if (fd < 0) {
      return;
    }

    for (int attempt = 0; attempt < 10; attempt++) {
      drmModeRes *res = drmModeGetResources(fd);
      if (!res) {
        BOOST_LOG(debug) << "[VDISPLAY] DRM connector probe skipped for " << context << ": " << strerror(errno);
        return;
      }

      bool found_modes = false;
      for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector = drmModeGetConnector(fd, res->connectors[i]);
        if (!connector) {
          continue;
        }

        BOOST_LOG(debug) << "[VDISPLAY] Probed connector " << connector->connector_id
                         << " on " << context << ": connection=" << connector->connection
                         << ", modes=" << connector->count_modes;

        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
          BOOST_LOG(info) << "[VDISPLAY] DRM connector " << connector->connector_id
                          << " on " << context << " exposed " << connector->count_modes << " modes";
          found_modes = true;
        }

        drmModeFreeConnector(connector);
      }

      drmModeFreeResources(res);

      if (found_modes) {
        return;
      }

      std::this_thread::sleep_for(100ms);
    }

    BOOST_LOG(debug) << "[VDISPLAY] DRM connector probe found no modes for " << context;
  }

  static bool apply_mutter_display_config(uint32_t width, uint32_t height, uint32_t refresh_hz, bool isolate) {
    constexpr const char *script_path = "/tmp/apollo-mutter-displayconfig.py";

    std::ofstream script(script_path, std::ios::trunc);
    if (!script) {
      BOOST_LOG(warning) << "[VDISPLAY] Could not write Mutter display helper: " << strerror(errno);
      return false;
    }

    script << R"PY(#!/usr/bin/env python3
import sys
import time

from gi.repository import Gio, GLib

target_width = int(sys.argv[1])
target_height = int(sys.argv[2])
target_refresh = int(sys.argv[3])
isolate = sys.argv[4] == "1"


def monitor_key(spec):
    return tuple(spec)


def is_apollo_monitor(monitor):
    spec, _modes, props = monitor
    connector, vendor, product, _serial = spec
    display_name = str(props.get("display-name", ""))
    haystack = " ".join([connector, vendor, product, display_name]).upper()
    return (
        vendor.upper() == "APL"
        or "APOLLO" in haystack
        or "VDISP" in haystack
    )


def mode_prop(mode, name):
    props = mode[6]
    return bool(props.get(name, False))


def pick_mode(monitor, width=None, height=None, refresh=None):
    modes = monitor[1]
    if width and height:
        exact = [
            mode for mode in modes
            if mode[1] == width
            and mode[2] == height
            and (not refresh or abs(float(mode[3]) - float(refresh)) < 1.0)
        ]
        if exact:
            return exact[0]

        size_match = [mode for mode in modes if mode[1] == width and mode[2] == height]
        if size_match:
            return size_match[0]

    for mode in modes:
        if mode_prop(mode, "is-current"):
            return mode

    for mode in modes:
        if mode_prop(mode, "is-preferred"):
            return mode

    return modes[0] if modes else None


def get_state_for_name(name):
    return bus.call_sync(
        name,
        "/org/gnome/Mutter/DisplayConfig",
        "org.gnome.Mutter.DisplayConfig",
        "GetCurrentState",
        None,
        None,
        Gio.DBusCallFlags.NO_AUTO_START,
        120,
        None,
    ).unpack()


bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)

dbus_proxy = Gio.DBusProxy.new_sync(
    bus,
    Gio.DBusProxyFlags.NONE,
    None,
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    None,
)


def bus_names_for_gnome_shell():
    names = ["org.gnome.Mutter.DisplayConfig"]
    try:
        all_names = dbus_proxy.call_sync(
            "ListNames",
            None,
            Gio.DBusCallFlags.NONE,
            1000,
            None,
        ).unpack()[0]
    except Exception:
        return names

    for name in all_names:
        if not name.startswith(":"):
            continue

        try:
            pid = dbus_proxy.call_sync(
                "GetConnectionUnixProcessID",
                GLib.Variant("(s)", (name,)),
                Gio.DBusCallFlags.NONE,
                300,
                None,
            ).unpack()[0]
            with open(f"/proc/{pid}/comm", "r", encoding="utf-8") as comm_file:
                if comm_file.read().strip() == "gnome-shell":
                    names.append(name)
        except Exception:
            continue

    return list(dict.fromkeys(names))


state = None
apollo_monitor = None
proxy_name = None
candidate_names = bus_names_for_gnome_shell()
for _attempt in range(12):
    for candidate_name in candidate_names:
        try:
            candidate_state = get_state_for_name(candidate_name)
        except Exception:
            continue

        for monitor in candidate_state[1]:
            if is_apollo_monitor(monitor):
                proxy_name = candidate_name
                state = candidate_state
                apollo_monitor = monitor
                break

        if apollo_monitor:
            break

    if apollo_monitor:
        break

    time.sleep(0.1)

if not apollo_monitor:
    print("[VDISPLAY] Apollo EVDI monitor was not visible to Mutter", file=sys.stderr)
    sys.exit(2)

serial, monitors, logical_monitors, properties = state
monitor_by_spec = {monitor_key(monitor[0]): monitor for monitor in monitors}
apollo_spec = apollo_monitor[0]
apollo_mode = pick_mode(apollo_monitor, target_width, target_height, target_refresh)

if not apollo_mode:
    print("[VDISPLAY] Apollo EVDI monitor has no modes", file=sys.stderr)
    sys.exit(3)

layout_mode = int(properties.get("layout-mode", 1))
logical_config = [
    (
        0,
        0,
        float(apollo_mode[4]),
        0,
        True,
        [(apollo_spec[0], apollo_mode[0], {})],
    )
]

next_x = int(apollo_mode[1])
if not isolate:
    added = {monitor_key(apollo_spec)}
    for logical in logical_monitors:
        scale = float(logical[2])
        transform = int(logical[3])
        for spec in logical[5]:
            key = monitor_key(spec)
            if key in added:
                continue

            monitor = monitor_by_spec.get(key)
            if not monitor:
                continue

            mode = pick_mode(monitor)
            if not mode:
                continue

            logical_config.append(
                (
                    next_x,
                    0,
                    scale,
                    transform,
                    False,
                    [(spec[0], mode[0], {})],
                )
            )
            next_x += int(mode[1])
            added.add(key)

params = GLib.Variant(
    "(uua(iiduba(ssa{sv}))a{sv})",
    (
        int(serial),
        1,
        logical_config,
        {"layout-mode": GLib.Variant("u", layout_mode)},
    ),
)

bus.call_sync(
    proxy_name,
    "/org/gnome/Mutter/DisplayConfig",
    "org.gnome.Mutter.DisplayConfig",
    "ApplyMonitorsConfig",
    params,
    None,
    Gio.DBusCallFlags.NONE,
    3000,
    None,
)

print(
    f"[VDISPLAY] Applied Mutter monitor layout: Apollo primary "
    f"{apollo_mode[1]}x{apollo_mode[2]}@{float(apollo_mode[3]):.3f}Hz, "
    f"isolate={isolate}, bus={proxy_name}"
)
)PY";
    script.close();

    if (!script) {
      BOOST_LOG(warning) << "[VDISPLAY] Could not finish writing Mutter display helper: " << strerror(errno);
      return false;
    }

    std::ostringstream command;
    command << "timeout 5s python3 " << script_path << ' '
            << width << ' '
            << height << ' '
            << refresh_hz << ' '
            << (isolate ? 1 : 0);

    int result = std::system(command.str().c_str());
    if (result == 0) {
      BOOST_LOG(info) << "[VDISPLAY] Mutter display layout applied for EVDI virtual display.";
      return true;
    }

    BOOST_LOG(warning) << "[VDISPLAY] Mutter display layout helper failed with status " << result;
    return false;
  }

  static void create_detailed_timing_descriptor(unsigned char *dtd, uint32_t width, uint32_t height, uint32_t refresh_rate) {
    // Calculate timing parameters based on CVT (Coordinated Video Timings)
    // These are approximate values for common resolutions

    uint32_t h_blank, v_blank, h_front, h_sync, v_front, v_sync;
    uint32_t pixel_clock_khz;

    if (width == 3840 && height == 2160) {
      // 4K UHD @ 60Hz
      h_blank = 560;
      v_blank = 90;
      h_front = 176;
      h_sync = 88;
      v_front = 8;
      v_sync = 10;
      pixel_clock_khz = 533250; // 533.25 MHz
    } else if (width == 2560 && height == 1440) {
      // 1440p @ 60Hz
      h_blank = 160;
      v_blank = 44;
      h_front = 48;
      h_sync = 32;
      v_front = 3;
      v_sync = 5;
      pixel_clock_khz = 241500; // 241.5 MHz
    } else if (width == 1920 && height == 1080) {
      // 1080p @ 60Hz
      h_blank = 280;
      v_blank = 45;
      h_front = 88;
      h_sync = 44;
      v_front = 4;
      v_sync = 5;
      pixel_clock_khz = 148500; // 148.5 MHz
    } else if (width == 1280 && height == 720) {
      // 720p @ 60Hz
      h_blank = 370;
      v_blank = 30;
      h_front = 110;
      h_sync = 40;
      v_front = 5;
      v_sync = 5;
      pixel_clock_khz = 74250; // 74.25 MHz
    } else {
      // Generic calculation for other resolutions
      // Using simplified CVT formula
      double h_period = (1000000.0 / refresh_rate - 550) / (height + 3);
      double h_total = width + (width * 0.15); // ~15% horizontal blanking
      pixel_clock_khz = static_cast<uint32_t>((h_total / h_period) * 1000);
      h_blank = static_cast<uint32_t>(width * 0.15);
      v_blank = 45;
      h_front = h_blank / 4;
      h_sync = h_blank / 4;
      v_front = 3;
      v_sync = 5;
    }

    uint32_t h_active = width;
    uint32_t v_active = height;

    // Pixel clock in 10kHz units
    uint16_t pixel_clock = pixel_clock_khz / 10;

    // Detailed Timing Descriptor format (18 bytes)
    dtd[0] = pixel_clock & 0xFF;
    dtd[1] = (pixel_clock >> 8) & 0xFF;

    dtd[2] = h_active & 0xFF;
    dtd[3] = h_blank & 0xFF;
    dtd[4] = ((h_active >> 8) & 0x0F) << 4 | ((h_blank >> 8) & 0x0F);

    dtd[5] = v_active & 0xFF;
    dtd[6] = v_blank & 0xFF;
    dtd[7] = ((v_active >> 8) & 0x0F) << 4 | ((v_blank >> 8) & 0x0F);

    dtd[8] = h_front & 0xFF;
    dtd[9] = h_sync & 0xFF;
    dtd[10] = ((v_front & 0x0F) << 4) | (v_sync & 0x0F);
    dtd[11] = (((h_front >> 8) & 0x03) << 6) | (((h_sync >> 8) & 0x03) << 4) |
              (((v_front >> 4) & 0x03) << 2) | ((v_sync >> 4) & 0x03);

    // Physical size (approximate based on 27" diagonal for 4K, scaled for others)
    uint32_t h_size_mm = (width * 600) / 3840;  // 600mm for 4K width
    uint32_t v_size_mm = (height * 340) / 2160; // 340mm for 4K height
    dtd[12] = h_size_mm & 0xFF;
    dtd[13] = v_size_mm & 0xFF;
    dtd[14] = ((h_size_mm >> 8) & 0x0F) << 4 | ((v_size_mm >> 8) & 0x0F);

    dtd[15] = 0; // No border
    dtd[16] = 0; // No border
    dtd[17] = 0x1E; // Digital separate sync, positive H and V
  }

  static unsigned char *generate_edid_for_resolution(uint32_t width, uint32_t height, uint32_t refresh_rate) {
    static unsigned char edid[256]; // Support for 1 extension block
    memset(edid, 0, sizeof(edid));

    // Block 0: Base EDID
    // Header
    edid[0] = 0x00;
    edid[1] = 0xFF; edid[2] = 0xFF; edid[3] = 0xFF;
    edid[4] = 0xFF; edid[5] = 0xFF; edid[6] = 0xFF;
    edid[7] = 0x00;

    // Manufacturer ID: "APL" (Apollo)
    edid[8] = 0x06; edid[9] = 0x4C;

    // Product code
    edid[10] = 0x01; edid[11] = 0x00;

    // Serial number
    edid[12] = 0x01; edid[13] = 0x00; edid[14] = 0x00; edid[15] = 0x00;

    // Week and year of manufacture (week 1, 2024)
    edid[16] = 0x01; edid[17] = 0x22;

    // EDID version 1.4
    edid[18] = 0x01; edid[19] = 0x04;

    // Video input: Digital, 8-bit color, DisplayPort
    edid[20] = 0xB5;

    // Screen size (cm) - approximate for 27"
    edid[21] = 60; // 60 cm wide
    edid[22] = 34; // 34 cm tall

    // Gamma (2.2)
    edid[23] = 0x78;

    // Features: RGB, preferred timing in DTD1
    edid[24] = 0x3A;

    // Chromaticity coordinates (standard sRGB)
    edid[25] = 0xFC; edid[26] = 0x81; edid[27] = 0xA4; edid[28] = 0x55;
    edid[29] = 0x4D; edid[30] = 0x9D; edid[31] = 0x25; edid[32] = 0x12;
    edid[33] = 0x50; edid[34] = 0x54;

    // Established timings
    edid[35] = 0x21; edid[36] = 0x08; edid[37] = 0x00;

    // Standard timings (8 entries, 2 bytes each)
    // We'll add some common resolutions
    edid[38] = 0xD1; edid[39] = 0xC0; // 1920x1080@60
    edid[40] = 0xB3; edid[41] = 0x00; // 1680x1050@60
    edid[42] = 0xA9; edid[43] = 0xC0; // 1600x900@60
    edid[44] = 0x81; edid[45] = 0x80; // 1280x1024@60
    edid[46] = 0x81; edid[47] = 0xC0; // 1280x720@60
    edid[48] = 0x01; edid[49] = 0x01; // Unused
    edid[50] = 0x01; edid[51] = 0x01; // Unused
    edid[52] = 0x01; edid[53] = 0x01; // Unused

    // Detailed Timing Descriptor 1 (preferred timing)
    create_detailed_timing_descriptor(&edid[54], width, height, refresh_rate);

    // Descriptor 2: Display name
    edid[72] = 0x00; edid[73] = 0x00; edid[74] = 0x00; edid[75] = 0xFC; edid[76] = 0x00;
    const char *name = "APOLLO VDISP";
    for (int i = 0; i < 13 && name[i]; i++) {
      edid[77 + i] = name[i];
    }
    edid[89] = '\n';

    // Descriptor 3: Display range limits
    edid[90] = 0x00; edid[91] = 0x00; edid[92] = 0x00; edid[93] = 0xFD; edid[94] = 0x00;
    edid[95] = 0x18; // Min V rate: 24 Hz
    edid[96] = 0x78; // Max V rate: 120 Hz
    edid[97] = 0x0F; // Min H rate: 15 kHz
    edid[98] = 0xA0; // Max H rate: 160 kHz
    edid[99] = 0x78; // Max pixel clock: 1200 MHz (for 4K@120Hz support)
    edid[100] = 0x00; // GTF support
    edid[101] = 0x0A; // Newline padding
    for (int i = 102; i < 108; i++) edid[i] = 0x20; // Space padding

    // Descriptor 4: Dummy/unused
    edid[108] = 0x00; edid[109] = 0x00; edid[110] = 0x00; edid[111] = 0x10; edid[112] = 0x00;
    for (int i = 113; i < 126; i++) edid[i] = 0x20;

    // Extension flag: 1 extension block (for resolutions > 1080p)
    bool needs_extension = (width > 1920 || height > 1080);
    edid[126] = needs_extension ? 0x01 : 0x00;

    // Calculate checksum for block 0
    calculate_edid_checksum(edid, 128);

    // Block 1: CEA-861 Extension (for 4K support)
    if (needs_extension) {
      edid[128] = 0x02; // CEA extension tag
      edid[129] = 0x03; // Revision 3
      edid[130] = 0x18; // DTD offset (24 bytes for data blocks)
      edid[131] = 0x72; // Native DTDs, YCbCr support

      // Video Data Block
      edid[132] = 0x47; // Video tag (0x40) + length (7)
      edid[133] = 0x90; // VIC 16: 1080p60 (native)
      edid[134] = 0x04; // VIC 4: 720p60
      edid[135] = 0x03; // VIC 3: 480p60
      edid[136] = 0x5F; // VIC 95: 4K@60Hz (VIC 95)
      edid[137] = 0x60; // VIC 96: 4K@60Hz (VIC 96)
      edid[138] = 0x61; // VIC 97: 4K@60Hz (VIC 97)
      edid[139] = 0x65; // VIC 101: 4K@120Hz

      // HDMI Vendor Specific Data Block
      edid[140] = 0x67; // Vendor tag (0x60) + length (7)
      edid[141] = 0x03; // IEEE OUI for HDMI (0x000C03)
      edid[142] = 0x0C;
      edid[143] = 0x00;
      edid[144] = 0x10; // Source physical address
      edid[145] = 0x00;
      edid[146] = 0x00; // Supports AI, DC 48/36/30 bit
      edid[147] = 0x78; // Max TMDS clock / 5 MHz = 600 MHz

      // Detailed Timing Descriptor for 4K if needed
      if (width >= 3840) {
        create_detailed_timing_descriptor(&edid[152], 3840, 2160, 60);
      }

      // Calculate checksum for block 1
      calculate_edid_checksum(&edid[128], 128);
    }

    return edid;
  }

  // ============================================================================
  // Public API Implementation
  // ============================================================================

  DRIVER_STATUS openVDisplayDevice() {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (driver_status == DRIVER_STATUS::OK) {
      return driver_status;
    }

    BOOST_LOG(info) << "[VDISPLAY] Initializing Linux virtual display driver...";

    selected_backend = configured_backend();
    BOOST_LOG(info) << "[VDISPLAY] Requested Linux virtual display backend: " << linuxVirtualDisplayBackendName(selected_backend);

    if (selected_backend == BACKEND::MUTTER_PIPEWIRE) {
      evdi_available = false;
      if (!mutter_screencast_available()) {
        driver_status = DRIVER_STATUS::NOT_SUPPORTED;
        return driver_status;
      }

      driver_status = DRIVER_STATUS::OK;
      BOOST_LOG(info) << "[VDISPLAY] Linux virtual display driver initialized with Mutter/PipeWire.";
      return driver_status;
    }

    evdi_available = load_evdi_library();

    if (evdi_available) {
      // Check if kernel module is loaded
      if (!check_evdi_module_loaded()) {
        BOOST_LOG(warning) << "[VDISPLAY] EVDI library loaded but kernel module not available.";
        evdi_available = false;
      }
    }

    if (!evdi_available) {
      BOOST_LOG(error) << "[VDISPLAY] " << linuxVirtualDisplayBackendName(selected_backend)
                       << " backend requires EVDI, but EVDI is not available.";
      driver_status = DRIVER_STATUS::NOT_SUPPORTED;
      return driver_status;
    }

    if (selected_backend == BACKEND::EVDI_PIPEWIRE && !mutter_screencast_available()) {
      BOOST_LOG(error) << "[VDISPLAY] EVDI monitor/PipeWire backend requires Mutter ScreenCast.";
      driver_status = DRIVER_STATUS::NOT_SUPPORTED;
      return driver_status;
    }

    BOOST_LOG(info) << "[VDISPLAY] " << linuxVirtualDisplayBackendName(selected_backend) << " backend available.";
    driver_status = DRIVER_STATUS::OK;
    BOOST_LOG(info) << "[VDISPLAY] Linux virtual display driver initialized successfully.";

    return driver_status;
  }

  void closeVDisplayDevice() {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    BOOST_LOG(info) << "[VDISPLAY] Closing Linux virtual display driver...";

    // Stop watchdog thread
    watchdog_running = false;
    if (watchdog_thread.joinable()) {
      watchdog_thread.join();
    }

    // Clean up all virtual displays
    for (auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
        if (vdinfo.backend == BACKEND::MUTTER_PIPEWIRE) {
          destroy_mutter_virtual_stream(vdinfo);
        }
#endif
        if (vdinfo.using_evdi && vdinfo.handle) {
          if (vdinfo.painter) {
            vdinfo.painter->stop();
          }
          evdi.disconnect(vdinfo.handle);
          evdi.close(vdinfo.handle);
        }
        if (vdinfo.drm_fd >= 0) {
          ::close(vdinfo.drm_fd);
        }
      }
    }
    virtual_displays.clear();

    // Unload EVDI library
    unload_evdi_library();

    driver_status = DRIVER_STATUS::UNKNOWN;
    BOOST_LOG(info) << "[VDISPLAY] Linux virtual display driver closed.";
  }

  bool startPingThread(std::function<void()> failCb) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (watchdog_running) {
      return true;
    }

    watchdog_running = true;

    watchdog_thread = std::thread([failCb = std::move(failCb)]() {
      BOOST_LOG(debug) << "[VDISPLAY] Watchdog thread started.";

      while (watchdog_running) {
        std::this_thread::sleep_for(5s);

        if (!watchdog_running) {
          break;
        }

        std::lock_guard<std::mutex> lock(vdisplay_mutex);

        for (const auto &[guid, vdinfo] : virtual_displays) {
          if (vdinfo.active && vdinfo.using_evdi && vdinfo.handle) {
            // Check EVDI device health
            int ready = evdi.get_event_ready(vdinfo.handle);
            if (ready < 0) {
              BOOST_LOG(error) << "[VDISPLAY] Virtual display " << vdinfo.name << " lost!";
              if (failCb) {
                failCb();
              }
              return;
            }
          }
        }
      }

      BOOST_LOG(debug) << "[VDISPLAY] Watchdog thread stopped.";
    });

    return true;
  }

  bool setRenderAdapterByName(const std::string &adapterName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (adapterName.empty()) {
      BOOST_LOG(debug) << "[VDISPLAY] No specific adapter requested.";
      return true;
    }

    BOOST_LOG(info) << "[VDISPLAY] Adapter hint: " << adapterName;
    // On Linux, we don't need to select specific adapters for EVDI
    return true;
  }

  std::string createVirtualDisplay(
    const char *s_client_uid,
    const char *s_client_name,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    const uuid_util::uuid_t &guid
  ) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    if (driver_status != DRIVER_STATUS::OK) {
      BOOST_LOG(error) << "[VDISPLAY] Driver not initialized.";
      return "";
    }

    std::string guid_str = guid.string();
    std::string display_name = generate_display_name(guid);

    uint32_t fps_hz = fps;
    if (fps_hz == 0) {
      fps_hz = 60;
    } else if (fps_hz >= 1000) {
      fps_hz /= 1000;
    }

    BOOST_LOG(info) << "[VDISPLAY] Creating virtual display: " << display_name
                    << " (W: " << width << ", H: " << height << ", FPS: " << fps_hz << ")";
    BOOST_LOG(info) << "[VDISPLAY] Client: " << s_client_name << " (" << s_client_uid << ")";

    VirtualDisplayInfo vdinfo;
    vdinfo.name = display_name;
    vdinfo.guid_str = guid_str;
    vdinfo.width = width;
    vdinfo.height = height;
    vdinfo.fps = fps_hz * 1000;
    vdinfo.backend = selected_backend;
    vdinfo.device_index = -1;
    vdinfo.handle = nullptr;
    vdinfo.drm_fd = -1;
    vdinfo.active = true;
    vdinfo.using_evdi = false;

    if (selected_backend == BACKEND::MUTTER_PIPEWIRE) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
      if (!create_mutter_virtual_stream(vdinfo)) {
        BOOST_LOG(error) << "[VDISPLAY] Mutter/PipeWire virtual display creation failed for " << display_name;
        return "";
      }
#else
      BOOST_LOG(error) << "[VDISPLAY] Mutter/PipeWire backend is not compiled in.";
      return "";
#endif
    } else if ((selected_backend == BACKEND::EVDI || selected_backend == BACKEND::EVDI_PIPEWIRE) && evdi_available) {
      // Create real virtual display using EVDI
      int device = find_available_evdi_device();
      if (device >= 0) {
        evdi_handle handle = evdi.open(device);
        if (handle) {
          // Generate EDID for requested resolution
          unsigned char *edid = generate_edid_for_resolution(width, height, fps_hz);

          // Determine EDID size (128 for base, 256 with extension for 4K)
          unsigned int edid_size = (width > 1920 || height > 1080) ? 256 : 128;

          // Connect with EDID (no area limit)
          BOOST_LOG(info) << "[VDISPLAY] Connecting with " << edid_size << "-byte EDID for " << width << "x" << height;
          evdi.connect(handle, edid, edid_size, 0);

          vdinfo.device_index = device;
          vdinfo.handle = handle;
          vdinfo.using_evdi = true;
          vdinfo.backend = selected_backend;
          if (should_run_evdi_painter(vdinfo.backend)) {
            vdinfo.painter = std::make_shared<evdi_painter_t>();
            vdinfo.painter->start(
              vdinfo.handle,
              vdinfo.name,
              should_publish_evdi_painter_frames(vdinfo.backend)
            );
          } else {
            BOOST_LOG(info) << "[VDISPLAY] Skipping EVDI painter update pump for " << vdinfo.name
                            << " for backend " << linuxVirtualDisplayBackendName(vdinfo.backend) << '.';
          }

          // Find the DRM card for this EVDI device
          std::string card_path = "/dev/dri/card" + std::to_string(device);
          vdinfo.drm_fd = ::open(card_path.c_str(), O_RDWR);
          drop_drm_master_if_held(vdinfo.drm_fd, card_path);
          probe_drm_connectors(vdinfo.drm_fd, card_path);
          BOOST_LOG(info) << "[VDISPLAY] Created EVDI virtual display on device " << device;
        } else {
          BOOST_LOG(warning) << "[VDISPLAY] Failed to open EVDI device " << device;
        }
      } else {
        BOOST_LOG(warning) << "[VDISPLAY] No available EVDI device.";
      }
    }

    if ((selected_backend == BACKEND::EVDI || selected_backend == BACKEND::EVDI_PIPEWIRE) && !vdinfo.using_evdi) {
      BOOST_LOG(error) << "[VDISPLAY] " << linuxVirtualDisplayBackendName(selected_backend)
                       << " virtual display creation failed; refusing fallback to a physical display.";
      return "";
    }

    const bool using_evdi = vdinfo.using_evdi;
    const auto backend = vdinfo.backend;
    virtual_displays[guid_str] = std::move(vdinfo);

    BOOST_LOG(info) << "[VDISPLAY] Virtual display created successfully: " << display_name;
    BOOST_LOG(info) << "[VDISPLAY] Mode: " << linuxVirtualDisplayBackendName(backend)
                    << (using_evdi ? " (real EVDI monitor)" : "");

    return display_name;
  }

  bool removeVirtualDisplay(const uuid_util::uuid_t &guid) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    std::string guid_str = guid.string();

    auto it = virtual_displays.find(guid_str);
    if (it == virtual_displays.end()) {
      BOOST_LOG(warning) << "[VDISPLAY] Virtual display not found: " << guid_str;
      return false;
    }

    auto &vdinfo = it->second;
    BOOST_LOG(info) << "[VDISPLAY] Removing virtual display: " << vdinfo.name;

#ifdef SUNSHINE_BUILD_PIPEWIRE
    if (vdinfo.backend == BACKEND::MUTTER_PIPEWIRE) {
      destroy_mutter_virtual_stream(vdinfo);
    }
#endif

    if (vdinfo.using_evdi && vdinfo.handle) {
      if (vdinfo.painter) {
        vdinfo.painter->stop();
      }
      evdi.disconnect(vdinfo.handle);
      evdi.close(vdinfo.handle);
    }

    if (vdinfo.drm_fd >= 0) {
      ::close(vdinfo.drm_fd);
    }

    virtual_displays.erase(it);

    BOOST_LOG(info) << "[VDISPLAY] Virtual display removed successfully.";
    return true;
  }

  static int change_display_settings(const char *deviceName, int width, int height, int refresh_rate, bool isolate) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    // Convert from mHz to Hz
    int refresh_hz = refresh_rate;
    if (refresh_hz == 0) {
      refresh_hz = 60;
    } else if (refresh_hz >= 1000) {
      refresh_hz /= 1000;
    }

    BOOST_LOG(info) << "[VDISPLAY] Changing display settings for " << deviceName
                    << " to " << width << "x" << height << "@" << refresh_hz << "Hz";

    // Find the virtual display
    for (auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == deviceName) {
        bool mode_unchanged = vdinfo.width == static_cast<uint32_t>(width) &&
                              vdinfo.height == static_cast<uint32_t>(height) &&
                              vdinfo.fps == static_cast<uint32_t>(refresh_hz * 1000);

        vdinfo.width = width;
        vdinfo.height = height;
        vdinfo.fps = refresh_hz * 1000;

        if (vdinfo.backend == BACKEND::MUTTER_PIPEWIRE) {
          if (!mode_unchanged) {
            BOOST_LOG(warning) << "[VDISPLAY] Mutter/PipeWire virtual display mode changes require stream recreation; "
                               << vdinfo.name << " will keep its active PipeWire mode until the next session.";
          }
          BOOST_LOG(info) << "[VDISPLAY] Mutter/PipeWire virtual display mode recorded successfully.";
          return 0;
        }

        if (vdinfo.using_evdi && vdinfo.handle) {
          if (!mode_unchanged) {
            if (vdinfo.painter) {
              vdinfo.painter->stop();
            }

            // Reconnect with new EDID for new resolution
            evdi.disconnect(vdinfo.handle);
            unsigned char *edid = generate_edid_for_resolution(width, height, refresh_hz);
            unsigned int edid_size = (width > 1920 || height > 1080) ? 256 : 128;
            BOOST_LOG(info) << "[VDISPLAY] Reconnecting with " << edid_size << "-byte EDID for " << width << "x" << height;
            evdi.connect(vdinfo.handle, edid, edid_size, 0);

            if (vdinfo.painter) {
              vdinfo.painter->start(
                vdinfo.handle,
                vdinfo.name,
                should_publish_evdi_painter_frames(vdinfo.backend)
              );
            } else if (should_run_evdi_painter(vdinfo.backend)) {
              vdinfo.painter = std::make_shared<evdi_painter_t>();
              vdinfo.painter->start(
                vdinfo.handle,
                vdinfo.name,
                should_publish_evdi_painter_frames(vdinfo.backend)
              );
            } else {
              BOOST_LOG(info) << "[VDISPLAY] Keeping EVDI painter update pump disabled for " << vdinfo.name
                              << " for backend " << linuxVirtualDisplayBackendName(vdinfo.backend) << '.';
            }
          }

	          probe_drm_connectors(vdinfo.drm_fd, "/dev/dri/card" + std::to_string(vdinfo.device_index));
	          apply_mutter_display_config(width, height, refresh_hz, isolate);
	          if (vdinfo.backend == BACKEND::EVDI_PIPEWIRE) {
	            std::this_thread::sleep_for(750ms);
	          }
	        }

        BOOST_LOG(info) << "[VDISPLAY] Display settings updated successfully.";
        return 0;
      }
    }

    BOOST_LOG(debug) << "[VDISPLAY] Display not found: " << deviceName;
    return 0;
  }

  int changeDisplaySettings(const char *deviceName, int width, int height, int refresh_rate) {
    return change_display_settings(deviceName, width, height, refresh_rate, false);
  }

  int changeDisplaySettings2(const char *deviceName, int width, int height, int refresh_rate, bool bApplyIsolated) {
    if (bApplyIsolated) {
      BOOST_LOG(debug) << "[VDISPLAY] Applying isolated virtual display layout.";
    }
    return change_display_settings(deviceName, width, height, refresh_rate, bApplyIsolated);
  }

  std::string getPrimaryDisplay() {
    // Return first connected physical display
    try {
      for (const auto &entry : fs::directory_iterator("/dev/dri")) {
        const auto &path = entry.path();
        std::string filename = path.filename().string();
        if (filename.find("card") == 0 && filename.find("render") == std::string::npos) {
          int fd = ::open(path.c_str(), O_RDWR);
          if (fd >= 0) {
            drmModeRes *res = drmModeGetResources(fd);
            if (res) {
              for (int i = 0; i < res->count_connectors; i++) {
                drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
                if (conn && conn->connection == DRM_MODE_CONNECTED) {
                  std::string name = "HDMI-A-" + std::to_string(conn->connector_type_id);
                  drmModeFreeConnector(conn);
                  drmModeFreeResources(res);
                  ::close(fd);
                  return name;
                }
                if (conn) drmModeFreeConnector(conn);
              }
              drmModeFreeResources(res);
            }
            ::close(fd);
          }
        }
      }
    } catch (...) {}
    return "";
  }

  bool setPrimaryDisplay(const char *primaryDeviceName) {
    BOOST_LOG(debug) << "[VDISPLAY] setPrimaryDisplay is a no-op on Linux.";
    return true;
  }

  bool getDisplayHDRByName(const char *displayName) {
    BOOST_LOG(debug) << "[VDISPLAY] HDR check for: " << displayName;
    // EVDI doesn't support HDR currently
    return false;
  }

  bool setDisplayHDRByName(const char *displayName, bool enableAdvancedColor) {
    BOOST_LOG(debug) << "[VDISPLAY] HDR setting not supported on Linux/EVDI.";
    return false;
  }

  std::vector<std::string> matchDisplay(const std::string &sMatch) {
    std::vector<std::string> matches;

    std::lock_guard<std::mutex> lock(vdisplay_mutex);

    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name.find(sMatch) != std::string::npos) {
        matches.push_back(vdinfo.name);
      }
    }

    return matches;
  }

  bool isVirtualDisplay(const std::string &displayName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name == displayName) {
        return true;
      }
    }
    return false;
  }

  BACKEND virtualDisplayBackend(const std::string &displayName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name == displayName) {
        return vdinfo.backend;
      }
    }
    return BACKEND::UNKNOWN;
  }

  bool getVirtualDisplayMode(const std::string &displayName, uint32_t &width, uint32_t &height, uint32_t &fps) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name == displayName) {
        width = vdinfo.width;
        height = vdinfo.height;
        fps = vdinfo.fps;
        return true;
      }
    }
    return false;
  }

  bool getMutterPipeWireNodeId(const std::string &displayName, uint32_t &node_id) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.active && vdinfo.name == displayName && vdinfo.backend == BACKEND::MUTTER_PIPEWIRE && vdinfo.pipewire_node_id) {
        node_id = vdinfo.pipewire_node_id;
        return true;
      }
    }
#else
    (void) displayName;
    (void) node_id;
#endif
    return false;
  }

#ifdef SUNSHINE_BUILD_PIPEWIRE
  struct mutter_remote_desktop_target_t {
    GDBusConnection *bus {};
    std::string session_path;
    std::string stream_path;
    uint32_t width {};
    uint32_t height {};
  };

  static bool get_active_mutter_remote_desktop_target(mutter_remote_desktop_target_t &target, bool require_stream) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (!vdinfo.active ||
          vdinfo.backend != BACKEND::MUTTER_PIPEWIRE ||
          !vdinfo.mutter_bus ||
          vdinfo.mutter_remote_desktop_session_path.empty()) {
        continue;
      }
      if (require_stream && vdinfo.mutter_stream_path.empty()) {
        continue;
      }

      target.bus = vdinfo.mutter_bus;
      g_object_ref(target.bus);
      target.session_path = vdinfo.mutter_remote_desktop_session_path;
      target.stream_path = vdinfo.mutter_stream_path;
      target.width = vdinfo.width;
      target.height = vdinfo.height;
      return true;
    }

    return false;
  }

  static bool queue_mutter_remote_desktop_event(mutter_remote_desktop_target_t &target, const char *method, GVariant *parameters) {
    if (!target.bus) {
      return false;
    }

    g_dbus_connection_call(
      target.bus,
      mutter_dbus::REMOTE_DESKTOP_SERVICE,
      target.session_path.c_str(),
      "org.gnome.Mutter.RemoteDesktop.Session",
      method,
      parameters,
      nullptr,
      G_DBUS_CALL_FLAGS_NONE,
      1000,
      nullptr,
      nullptr,
      nullptr
    );
    g_object_unref(target.bus);
    target.bus = nullptr;
    return true;
  }
#endif

  bool notifyMutterPointerMotionRelative(double dx, double dy) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
    mutter_remote_desktop_target_t target;
    if (!get_active_mutter_remote_desktop_target(target, false)) {
      return false;
    }

    return queue_mutter_remote_desktop_event(
      target,
      "NotifyPointerMotionRelative",
      g_variant_new("(dd)", dx, dy)
    );
#else
    (void) dx;
    (void) dy;
    return false;
#endif
  }

  bool notifyMutterPointerMotionAbsolute(double x, double y) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
    GDBusConnection *bus {};
    std::string session_path;
    std::string stream_path;
    double clamped_x {};
    double clamped_y {};
    {
      std::lock_guard<std::mutex> lock(vdisplay_mutex);
      for (const auto &[guid, vdinfo] : virtual_displays) {
        if (!vdinfo.active ||
            vdinfo.backend != BACKEND::MUTTER_PIPEWIRE ||
            !vdinfo.mutter_bus ||
            vdinfo.mutter_remote_desktop_session_path.empty() ||
            vdinfo.mutter_stream_path.empty()) {
          continue;
        }

        bus = vdinfo.mutter_bus;
        g_object_ref(bus);
        session_path = vdinfo.mutter_remote_desktop_session_path;
        stream_path = vdinfo.mutter_stream_path;
        clamped_x = std::clamp(x, 0.0, static_cast<double>(vdinfo.width));
        clamped_y = std::clamp(y, 0.0, static_cast<double>(vdinfo.height));
        break;
      }
    }

    if (!bus) {
      return false;
    }

    mutter_remote_desktop_target_t target;
    target.bus = bus;
    target.session_path = session_path;
    return queue_mutter_remote_desktop_event(
      target,
      "NotifyPointerMotionAbsolute",
      g_variant_new("(sdd)", stream_path.c_str(), clamped_x, clamped_y)
    );
#else
    (void) x;
    (void) y;
    return false;
#endif
  }

  bool notifyMutterPointerButton(int button, bool release) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
    mutter_remote_desktop_target_t target;
    if (!get_active_mutter_remote_desktop_target(target, false)) {
      return false;
    }

    return queue_mutter_remote_desktop_event(
      target,
      "NotifyPointerButton",
      g_variant_new("(ib)", button, !release)
    );
#else
    (void) button;
    (void) release;
    return false;
#endif
  }

  bool notifyMutterPointerAxis(double dx, double dy) {
#ifdef SUNSHINE_BUILD_PIPEWIRE
    constexpr uint32_t source_wheel = 2;

    mutter_remote_desktop_target_t target;
    if (!get_active_mutter_remote_desktop_target(target, false)) {
      return false;
    }

    return queue_mutter_remote_desktop_event(
      target,
      "NotifyPointerAxis",
      g_variant_new("(ddu)", dx, dy, source_wheel)
    );
#else
    (void) dx;
    (void) dy;
    return false;
#endif
  }

  // ============================================================================
  // EVDI-specific functions for KMS integration
  // ============================================================================

  /**
   * @brief Check if a display name is an EVDI virtual display.
   */
  bool isEvdiDisplay(const std::string &displayName) {
    if (!evdi_available) {
      return false;
    }

    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == displayName && vdinfo.using_evdi) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Get the DRM card index for an EVDI display.
   * @return Card index, or -1 if not found.
   */
  int getEvdiCardIndex(const std::string &displayName) {
    std::lock_guard<std::mutex> lock(vdisplay_mutex);
    for (const auto &[guid, vdinfo] : virtual_displays) {
      if (vdinfo.name == displayName && vdinfo.using_evdi) {
        return vdinfo.device_index;
      }
    }
    return -1;
  }

  bool copyLatestEvdiFrame(
    const std::string &displayName,
    std::uint8_t *dst,
    int width,
    int height,
    int dst_stride,
    std::chrono::milliseconds timeout,
    std::uint64_t &last_generation,
    std::chrono::steady_clock::time_point &frame_timestamp,
    bool require_new_frame
  ) {
    std::shared_ptr<evdi_painter_t> painter;
    {
      std::unique_lock<std::mutex> lock(vdisplay_mutex);
      for (auto &[guid, vdinfo] : virtual_displays) {
        if (vdinfo.name != displayName || !vdinfo.using_evdi || !vdinfo.painter) {
          continue;
        }

        painter = vdinfo.painter;
        break;
      }
    }

    return painter && painter->copy_latest_frame(dst, width, height, dst_stride, timeout, last_generation, frame_timestamp, require_new_frame);
  }

  bool isEvdiGrabBusy(const std::string &displayName, std::chrono::milliseconds minimum_duration) {
    std::shared_ptr<evdi_painter_t> painter;
    {
      std::unique_lock<std::mutex> lock(vdisplay_mutex);
      for (auto &[guid, vdinfo] : virtual_displays) {
        if (vdinfo.name != displayName || !vdinfo.using_evdi || !vdinfo.painter) {
          continue;
        }

        painter = vdinfo.painter;
        break;
      }
    }

    return painter && painter->grab_busy_for(minimum_duration);
  }

}  // namespace VDISPLAY
