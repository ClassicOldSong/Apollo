/**
 * @file src/audio.cpp
 * @brief Definitions for audio capture and encoding.
 */
// standard includes
#include <chrono>
#include <ctime>
#include <deque>
#include <mutex>
#include <thread>

// lib includes
#include <opus/opus_multistream.h>

// local includes
#include "audio.h"
#include "config.h"
#include "globals.h"
#include "logging.h"
#include "platform/common.h"
#include "thread_safe.h"
#include "utility.h"

namespace audio {
  using namespace std::literals;
  using opus_t = util::safe_ptr<OpusMSEncoder, opus_multistream_encoder_destroy>;
  using sample_queue_t = std::shared_ptr<safe::queue_t<std::vector<float>>>;

  namespace {
    struct mic_debug_state_t {
      std::mutex mutex;
      mic_debug_snapshot_t snapshot;
      std::chrono::steady_clock::time_point last_packet_time {};
      std::chrono::steady_clock::time_point last_decode_time {};
      std::chrono::steady_clock::time_point last_render_time {};
      bool has_last_packet_time {false};
      bool has_last_decode_time {false};
      bool has_last_render_time {false};
      std::deque<std::string> recent_events;
    };

    mic_debug_state_t &mic_debug_state() {
      static mic_debug_state_t state;
      return state;
    }

    void append_mic_event(mic_debug_state_t &state, const std::string &message) {
      const auto now = std::chrono::system_clock::now();
      const auto tt = std::chrono::system_clock::to_time_t(now);
      std::tm tm {};
#ifdef _WIN32
      localtime_s(&tm, &tt);
#else
      localtime_r(&tt, &tm);
#endif
      char timestamp[16] {};
      std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm);
      state.recent_events.push_front(std::string {timestamp} + " " + message);
      while (state.recent_events.size() > 12) {
        state.recent_events.pop_back();
      }
    }

    void set_mic_state_locked(mic_debug_state_t &state, const std::string &status) {
      state.snapshot.state = status;
      append_mic_event(state, status);
    }

    audio_ctx_ref_t &mic_redirect_audio_ctx() {
      static audio_ctx_ref_t ref;
      return ref;
    }
  }  // namespace

  static int start_audio_control(audio_ctx_t &ctx);
  static void stop_audio_control(audio_ctx_t &);
  static void apply_surround_params(opus_stream_config_t &stream, const stream_params_t &params);

  int map_stream(int channels, bool quality);

  constexpr auto SAMPLE_RATE = 48000;

  // NOTE: If you adjust the bitrates listed here, make sure to update the
  // corresponding bitrate adjustment logic in rtsp_stream::cmd_announce()
  opus_stream_config_t stream_configs[MAX_STREAM_CONFIG] {
    {
      SAMPLE_RATE,
      2,
      1,
      1,
      platf::speaker::map_stereo,
      96000,
    },
    {
      SAMPLE_RATE,
      2,
      1,
      1,
      platf::speaker::map_stereo,
      512000,
    },
    {
      SAMPLE_RATE,
      6,
      4,
      2,
      platf::speaker::map_surround51,
      256000,
    },
    {
      SAMPLE_RATE,
      6,
      6,
      0,
      platf::speaker::map_surround51,
      1536000,
    },
    {
      SAMPLE_RATE,
      8,
      5,
      3,
      platf::speaker::map_surround71,
      450000,
    },
    {
      SAMPLE_RATE,
      8,
      8,
      0,
      platf::speaker::map_surround71,
      2048000,
    },
  };

  void encodeThread(sample_queue_t samples, config_t config, void *channel_data) {
    auto packets = mail::man->queue<packet_t>(mail::audio_packets);
    auto stream = stream_configs[map_stream(config.channels, config.flags[config_t::HIGH_QUALITY])];
    if (config.flags[config_t::CUSTOM_SURROUND_PARAMS]) {
      apply_surround_params(stream, config.customStreamParams);
    }

    // Encoding takes place on this thread
    platf::adjust_thread_priority(platf::thread_priority_e::high);

    opus_t opus {opus_multistream_encoder_create(
      stream.sampleRate,
      stream.channelCount,
      stream.streams,
      stream.coupledStreams,
      stream.mapping,
      OPUS_APPLICATION_RESTRICTED_LOWDELAY,
      nullptr
    )};

    opus_multistream_encoder_ctl(opus.get(), OPUS_SET_BITRATE(stream.bitrate));
    opus_multistream_encoder_ctl(opus.get(), OPUS_SET_VBR(0));

    BOOST_LOG(info) << "Opus initialized: "sv << stream.sampleRate / 1000 << " kHz, "sv
                    << stream.channelCount << " channels, "sv
                    << stream.bitrate / 1000 << " kbps (total), LOWDELAY"sv;

    auto frame_size = config.packetDuration * stream.sampleRate / 1000;
    while (auto sample = samples->pop()) {
      buffer_t packet {1400};

      int bytes = opus_multistream_encode_float(opus.get(), sample->data(), frame_size, std::begin(packet), packet.size());
      if (bytes < 0) {
        BOOST_LOG(error) << "Couldn't encode audio: "sv << opus_strerror(bytes);
        packets->stop();

        return;
      }

      packet.fake_resize(bytes);
      packets->raise(channel_data, std::move(packet));
    }
  }

  void capture(safe::mail_t mail, config_t config, void *channel_data) {
    auto shutdown_event = mail->event<bool>(mail::shutdown);
    if (!config::audio.stream || config.input_only) {
      shutdown_event->view();
      return;
    }
    auto stream = stream_configs[map_stream(config.channels, config.flags[config_t::HIGH_QUALITY])];
    if (config.flags[config_t::CUSTOM_SURROUND_PARAMS]) {
      apply_surround_params(stream, config.customStreamParams);
    }

    auto ref = get_audio_ctx_ref();
    if (!ref) {
      return;
    }

    auto init_failure_fg = util::fail_guard([&shutdown_event]() {
      BOOST_LOG(error) << "Unable to initialize audio capture. The stream will not have audio."sv;

      // Wait for shutdown to be signalled if we fail init.
      // This allows streaming to continue without audio.
      shutdown_event->view();
      return;
    });

    auto &control = ref->control;
    if (!control) {
      return;
    }

    // Order of priority:
    // 1. Virtual sink
    // 2. Audio sink
    // 3. Host
    std::string *sink = &ref->sink.host;
    if (!config::audio.sink.empty()) {
      sink = &config::audio.sink;
    }

    // Prefer the virtual sink if host playback is disabled or there's no other sink
    if (ref->sink.null && (!config.flags[config_t::HOST_AUDIO] || sink->empty())) {
      auto &null = *ref->sink.null;
      switch (stream.channelCount) {
        case 2:
          sink = &null.stereo;
          break;
        case 6:
          sink = &null.surround51;
          break;
        case 8:
          sink = &null.surround71;
          break;
      }
    }

    BOOST_LOG(info) << "Selected audio sink: "sv << *sink;

    // Only the first to start a session may change the default sink
    if (!ref->sink_flag->exchange(true, std::memory_order_acquire)) {
      // If the selected sink is different than the current one, change sinks.
      ref->restore_sink = ref->sink.host != *sink;
      if (ref->restore_sink) {
        if (control->set_sink(*sink)) {
          return;
        }
      }
    }

    auto frame_size = config.packetDuration * stream.sampleRate / 1000;
    auto mic = control->microphone(stream.mapping, stream.channelCount, stream.sampleRate, frame_size);
    if (!mic) {
      return;
    }

    // Audio is initialized, so we don't want to print the failure message
    init_failure_fg.disable();

    // Capture takes place on this thread
    platf::adjust_thread_priority(platf::thread_priority_e::critical);

    auto samples = std::make_shared<sample_queue_t::element_type>(30);
    std::thread thread {encodeThread, samples, config, channel_data};

    auto fg = util::fail_guard([&]() {
      samples->stop();
      thread.join();

      shutdown_event->view();
    });

    int samples_per_frame = frame_size * stream.channelCount;

    while (!shutdown_event->peek()) {
      std::vector<float> sample_buffer;
      sample_buffer.resize(samples_per_frame);

      auto status = mic->sample(sample_buffer);
      switch (status) {
        case platf::capture_e::ok:
          break;
        case platf::capture_e::timeout:
          continue;
        case platf::capture_e::reinit:
          if (config::audio.auto_capture) {
            BOOST_LOG(info) << "Reinitializing audio capture"sv;
            mic.reset();
            do {
              mic = control->microphone(stream.mapping, stream.channelCount, stream.sampleRate, frame_size);
              if (!mic) {
                BOOST_LOG(warning) << "Couldn't re-initialize audio input"sv;
              }
            } while (!mic && !shutdown_event->view(5s));
          }

          continue;
        default:
          return;
      }

      samples->raise(std::move(sample_buffer));
    }
  }

  audio_ctx_ref_t get_audio_ctx_ref() {
    static auto control_shared {safe::make_shared<audio_ctx_t>(start_audio_control, stop_audio_control)};
    return control_shared.ref();
  }

  bool is_audio_ctx_sink_available(const audio_ctx_t &ctx) {
    if (!ctx.control) {
      return false;
    }

    const std::string &sink = ctx.sink.host.empty() ? config::audio.sink : ctx.sink.host;
    if (sink.empty()) {
      return false;
    }

    return ctx.control->is_sink_available(sink);
  }

  int init_mic_redirect_device() {
    auto &held_ref = mic_redirect_audio_ctx();
    if (!held_ref) {
      held_ref = get_audio_ctx_ref();
    }

    auto &ref = held_ref;
    if (!ref || !ref->control) {
      mic_debug_on_backend_error("Audio control is unavailable; microphone redirection could not initialize");
      return -1;
    }

    return ref->control->init_mic_redirect_device();
  }

  void release_mic_redirect_device() {
    auto &ref = mic_redirect_audio_ctx();
    if (!ref || !ref->control) {
      ref = {};
      return;
    }

    ref->control->release_mic_redirect_device();
    ref = {};
  }

  int write_mic_data(const char *data, std::size_t len, std::uint16_t sequence_number) {
    auto &held_ref = mic_redirect_audio_ctx();
    auto ref = held_ref ? held_ref : get_audio_ctx_ref();
    if (!ref || !ref->control) {
      BOOST_LOG(warning) << "Client microphone packet rejected before decode because audio control is unavailable"
                         << " [seq=" << sequence_number << ", len=" << len << ']';
      mic_debug_on_packet_dropped(sequence_number, "Audio control is unavailable while writing microphone data");
      return -1;
    }

    return ref->control->write_mic_data(data, len, sequence_number);
  }

  mic_debug_snapshot_t get_mic_debug_snapshot() {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);

    auto snapshot = state.snapshot;
    const auto now = std::chrono::steady_clock::now();
    if (state.has_last_packet_time) {
      snapshot.last_packet_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_packet_time).count();
    }
    if (state.has_last_decode_time) {
      snapshot.last_decode_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_decode_time).count();
    }
    if (state.has_last_render_time) {
      snapshot.last_render_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_render_time).count();
    }
    snapshot.recent_events.assign(state.recent_events.begin(), state.recent_events.end());
    snapshot.signal_detected = snapshot.last_input_level >= 0.02 && snapshot.last_decode_age_ms >= 0 && snapshot.last_decode_age_ms < 3000;
    return snapshot;
  }

  void mic_debug_on_session_start(const std::string &client_name, bool encryption_enabled) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot = {};
    state.snapshot.session_active = true;
    state.snapshot.mic_requested = true;
    state.snapshot.encryption_enabled = encryption_enabled;
    state.snapshot.client_name = client_name;
    state.snapshot.state = "Microphone redirection negotiated; waiting for client audio";
    state.snapshot.last_packet_age_ms = -1;
    state.snapshot.last_decode_age_ms = -1;
    state.snapshot.last_render_age_ms = -1;
    state.has_last_packet_time = false;
    state.has_last_decode_time = false;
    state.has_last_render_time = false;
    state.recent_events.clear();
    append_mic_event(state, "Microphone redirection negotiated for client [" + client_name + "]");
  }

  void mic_debug_on_session_stop(const std::string &reason) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.session_active = false;
    state.snapshot.decode_active = false;
    state.snapshot.render_active = false;
    state.snapshot.signal_detected = false;
    state.snapshot.state = reason.empty() ? "No active remote microphone session" : reason;
    append_mic_event(state, reason.empty() ? "Remote microphone session ended" : reason);
  }

  void mic_debug_on_backend_initialized(const std::string &backend_name) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.backend_initialized = true;
    state.snapshot.backend_name = backend_name;
    state.snapshot.last_error.clear();
    append_mic_event(state, "Microphone backend ready: " + backend_name);
  }

  void mic_debug_on_backend_target(const std::string &target_device_name, int channels, std::uint32_t sample_rate) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.target_device_name = target_device_name;
    state.snapshot.state = "Rendering client microphone into " + target_device_name;
    append_mic_event(state, "Using host render target [" + target_device_name + "] at " + std::to_string(channels) + "ch/" + std::to_string(sample_rate) + "Hz");
  }

  void mic_debug_on_backend_error(const std::string &message) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.last_error = message;
    state.snapshot.render_active = false;
    state.snapshot.state = message;
    append_mic_event(state, message);
  }

  void mic_debug_on_packet_received(std::uint16_t sequence_number, std::size_t payload_len) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.first_packet_received = true;
    state.snapshot.packets_received++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_payload_size = payload_len;
    state.last_packet_time = std::chrono::steady_clock::now();
    state.has_last_packet_time = true;
    if (state.snapshot.packets_received == 1) {
      set_mic_state_locked(state, "Receiving microphone packets from Moonlight");
    }
  }

  void mic_debug_on_packet_decrypt_error(std::uint16_t sequence_number, const std::string &message) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.decrypt_errors++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_error = message;
    state.snapshot.state = message;
    append_mic_event(state, message);
  }

  void mic_debug_on_packet_dropped(std::uint16_t sequence_number, const std::string &message) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.packets_dropped++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_error = message;
    state.snapshot.state = message;
    append_mic_event(state, message);
  }

  void mic_debug_on_packet_decoded(std::uint16_t sequence_number, double normalized_level, bool silent) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.decode_active = true;
    state.snapshot.packets_decoded++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_input_level = normalized_level;
    state.snapshot.last_error.clear();
    if (silent) {
      state.snapshot.silent_packets++;
    }
    state.last_decode_time = std::chrono::steady_clock::now();
    state.has_last_decode_time = true;
    if (state.snapshot.packets_decoded == 1) {
      set_mic_state_locked(state, "Apollo decoded microphone audio from Moonlight");
    }
  }

  void mic_debug_on_packet_rendered(std::uint16_t sequence_number, double normalized_level, bool silent) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.packets_rendered++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_render_level = normalized_level;
    state.snapshot.render_active = true;
    state.snapshot.last_error.clear();
    state.last_render_time = std::chrono::steady_clock::now();
    state.has_last_render_time = true;
    if (state.snapshot.packets_rendered == 1) {
      set_mic_state_locked(state, "Apollo is rendering microphone audio into VB-Cable");
    }
  }

  void mic_debug_on_decode_error(std::uint16_t sequence_number, const std::string &message) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.decode_errors++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_error = message;
    state.snapshot.state = message;
    append_mic_event(state, message);
  }

  void mic_debug_on_render_error(std::uint16_t sequence_number, const std::string &message) {
    auto &state = mic_debug_state();
    std::lock_guard lock(state.mutex);
    state.snapshot.render_errors++;
    state.snapshot.last_sequence_number = sequence_number;
    state.snapshot.last_error = message;
    state.snapshot.render_active = false;
    state.snapshot.state = message;
    append_mic_event(state, message);
  }

  int map_stream(int channels, bool quality) {
    int shift = quality ? 1 : 0;
    switch (channels) {
      case 2:
        return STEREO + shift;
      case 6:
        return SURROUND51 + shift;
      case 8:
        return SURROUND71 + shift;
    }
    return STEREO;
  }

  int start_audio_control(audio_ctx_t &ctx) {
    auto fg = util::fail_guard([]() {
      BOOST_LOG(warning) << "There will be no audio"sv;
    });

    ctx.sink_flag = std::make_unique<std::atomic_bool>(false);

    // The default sink has not been replaced yet.
    ctx.restore_sink = false;

    if (!(ctx.control = platf::audio_control())) {
      return 0;
    }

    auto sink = ctx.control->sink_info();
    if (!sink) {
      // Let the calling code know it failed
      ctx.control.reset();
      return 0;
    }

    ctx.sink = std::move(*sink);

    fg.disable();
    return 0;
  }

  void stop_audio_control(audio_ctx_t &ctx) {
    // restore audio-sink if applicable
    if (!ctx.restore_sink) {
      return;
    }

    // Change back to the host sink, unless there was none
    const std::string &sink = ctx.sink.host.empty() ? config::audio.sink : ctx.sink.host;
    if (!sink.empty()) {
      // Best effort, it's allowed to fail
      ctx.control->set_sink(sink);
    }
  }

  void apply_surround_params(opus_stream_config_t &stream, const stream_params_t &params) {
    stream.channelCount = params.channelCount;
    stream.streams = params.streams;
    stream.coupledStreams = params.coupledStreams;
    stream.mapping = params.mapping;
  }
}  // namespace audio
