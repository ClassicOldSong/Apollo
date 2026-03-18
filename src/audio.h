/**
 * @file src/audio.h
 * @brief Declarations for audio capture and encoding.
 */
#pragma once

// local includes
#include "platform/common.h"
#include "thread_safe.h"
#include "utility.h"

#include <cstddef>
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace audio {
  enum stream_config_e : int {
    STEREO,  ///< Stereo
    HIGH_STEREO,  ///< High stereo
    SURROUND51,  ///< Surround 5.1
    HIGH_SURROUND51,  ///< High surround 5.1
    SURROUND71,  ///< Surround 7.1
    HIGH_SURROUND71,  ///< High surround 7.1
    MAX_STREAM_CONFIG  ///< Maximum audio stream configuration
  };

  struct opus_stream_config_t {
    std::int32_t sampleRate;
    int channelCount;
    int streams;
    int coupledStreams;
    const std::uint8_t *mapping;
    int bitrate;
  };

  struct stream_params_t {
    int channelCount;
    int streams;
    int coupledStreams;
    std::uint8_t mapping[8];
  };

  extern opus_stream_config_t stream_configs[MAX_STREAM_CONFIG];

  struct config_t {
    enum flags_e : int {
      HIGH_QUALITY,  ///< High quality audio
      HOST_AUDIO,  ///< Host audio
      CUSTOM_SURROUND_PARAMS,  ///< Custom surround parameters
      MAX_FLAGS  ///< Maximum number of flags
    };

    int packetDuration;
    int channels;
    int mask;

    stream_params_t customStreamParams;

    std::bitset<MAX_FLAGS> flags;

    // Who TF knows what Sunshine did
    // putting input_only at the end of flags will always be over written to true
    uint64_t __padding;

    bool input_only;
  };

  struct audio_ctx_t {
    // We want to change the sink for the first stream only
    std::unique_ptr<std::atomic_bool> sink_flag;

    std::unique_ptr<platf::audio_control_t> control;

    bool restore_sink;
    platf::sink_t sink;
  };

  using buffer_t = util::buffer_t<std::uint8_t>;
  using packet_t = std::pair<void *, buffer_t>;
  using audio_ctx_ref_t = safe::shared_t<audio_ctx_t>::ptr_t;

  struct mic_debug_snapshot_t {
    bool session_active {};
    bool mic_requested {};
    bool encryption_enabled {};
    bool backend_initialized {};
    bool first_packet_received {};
    bool decode_active {};
    bool render_active {};
    bool signal_detected {};
    std::uint64_t packets_received {};
    std::uint64_t packets_decoded {};
    std::uint64_t packets_rendered {};
    std::uint64_t packets_dropped {};
    std::uint64_t decrypt_errors {};
    std::uint64_t decode_errors {};
    std::uint64_t render_errors {};
    std::uint64_t silent_packets {};
    std::uint16_t last_sequence_number {};
    std::size_t last_payload_size {};
    double last_input_level {};
    double last_render_level {};
    std::int64_t last_packet_age_ms {-1};
    std::int64_t last_decode_age_ms {-1};
    std::int64_t last_render_age_ms {-1};
    std::string client_name;
    std::string backend_name;
    std::string target_device_name;
    std::string endpoint_mix_format;
    std::string render_device_format;
    std::string render_format;
    std::string capture_device_name;
    std::string capture_endpoint_mix_format;
    std::string capture_device_format;
    std::string channel_mapping;
    std::string state;
    std::string last_error;
    bool resampling_active {};
    bool recommended_format_enforced {};
    bool recommended_format_active {};
    std::vector<std::string> recent_events;
  };

  void capture(safe::mail_t mail, config_t config, void *channel_data);

  /**
   * @brief Get the reference to the audio context.
   * @returns A shared pointer reference to audio context.
   * @note Aside from the configuration purposes, it can be used to extend the
   *       audio sink lifetime to capture sink earlier and restore it later.
   *
   * @examples
   * audio_ctx_ref_t audio = get_audio_ctx_ref()
   * @examples_end
   */
  audio_ctx_ref_t get_audio_ctx_ref();

  /**
   * @brief Check if the audio sink held by audio context is available.
   * @returns True if available (and can probably be restored), false otherwise.
   * @note Useful for delaying the release of audio context shared pointer (which
   *       tries to restore original sink).
   *
   * @examples
   * audio_ctx_ref_t audio = get_audio_ctx_ref()
   * if (audio.get()) {
   *     return is_audio_ctx_sink_available(*audio.get());
   * }
   * return false;
   * @examples_end
   */
  bool is_audio_ctx_sink_available(const audio_ctx_t &ctx);
  int init_mic_redirect_device();
  void release_mic_redirect_device();
  int write_mic_data(const char *data, std::size_t len, std::uint16_t sequence_number);
  mic_debug_snapshot_t get_mic_debug_snapshot();
  void mic_debug_on_session_start(const std::string &client_name, bool encryption_enabled);
  void mic_debug_on_session_stop(const std::string &reason = {});
  void mic_debug_on_backend_initialized(const std::string &backend_name);
  void mic_debug_on_backend_target(const std::string &target_device_name, int channels, std::uint32_t sample_rate);
  void mic_debug_on_backend_format(const std::string &endpoint_mix_format, const std::string &render_format, bool resampling_active, const std::string &channel_mapping);
  void mic_debug_on_backend_endpoint_formats(const std::string &render_device_format,
                                             const std::string &capture_device_name,
                                             const std::string &capture_endpoint_mix_format,
                                             const std::string &capture_device_format,
                                             bool recommended_format_enforced,
                                             bool recommended_format_active);
  void mic_debug_on_backend_error(const std::string &message);
  void mic_debug_on_packet_received(std::uint16_t sequence_number, std::size_t payload_len);
  void mic_debug_on_packet_decrypt_error(std::uint16_t sequence_number, const std::string &message);
  void mic_debug_on_packet_dropped(std::uint16_t sequence_number, const std::string &message);
  void mic_debug_on_packet_decoded(std::uint16_t sequence_number, double normalized_level, bool silent);
  void mic_debug_on_packet_rendered(std::uint16_t sequence_number, double normalized_level, bool silent);
  void mic_debug_on_decode_error(std::uint16_t sequence_number, const std::string &message);
  void mic_debug_on_render_error(std::uint16_t sequence_number, const std::string &message);
}  // namespace audio
