/**
 * @file src/platform/windows/mic_write.h
 * @brief Windows microphone redirection writer.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <Audioclient.h>
#include <mmdeviceapi.h>

#include "apollo_vmic.h"
#include "src/platform/common.h"

struct OpusDecoder;

namespace platf::audio {
  template<typename T>
  inline void release_com(T *ptr) {
    if (ptr) {
      ptr->Release();
    }
  }

  class mic_write_wasapi_t: public mic_redirect_backend_t {
  public:
    struct queued_mic_packet_t {
      std::vector<std::uint8_t> payload;
      std::uint16_t sequence_number {};
      std::uint32_t timestamp {};
      std::chrono::steady_clock::time_point arrival_time {};
    };

    mic_write_wasapi_t(std::string backend_name = "steam_streaming_microphone",
                       std::vector<std::wstring> autodetect_patterns = {},
                       std::string requested_device_name = {});
    ~mic_write_wasapi_t();

    std::string_view backend_id() const override;
    int init() override;
    int write_data(const char *data, std::size_t len, std::uint16_t sequence_number, std::uint32_t timestamp) override;
    void cleanup();

  private:
    bool initialize_device();
    bool find_target_device(EDataFlow flow, std::wstring &device_id, std::string &device_name);
    void render_loop();
    bool decode_next_packet();
    std::uint32_t infer_packet_duration_samples(std::uint32_t current_timestamp, std::uint32_t next_timestamp) const;
    bool should_conceal_missing_packet_locked() const;
    void append_decoded_frames(const float *samples, int decoded_frames, std::uint16_t sequence_number);

    util::safe_ptr<IMMDeviceEnumerator, release_com<IMMDeviceEnumerator>> device_enum;
    util::safe_ptr<IAudioClient, release_com<IAudioClient>> audio_client;
    IAudioRenderClient *audio_render = nullptr;
    OpusDecoder *opus_decoder = nullptr;
    std::vector<BYTE> active_format_storage;
    WAVEFORMATEX active_format {};
    UINT32 buffer_frame_count = 0;
    std::string backend_name;
    std::string requested_device_name;
    std::vector<std::wstring> autodetect_patterns;
    std::string target_device_name;
    bool first_packet_written_logged = false;
    util::safe_ptr_v2<void, BOOL, CloseHandle> render_event;
    std::mutex queue_mutex;
    std::map<std::uint16_t, queued_mic_packet_t> pending_packets;
    std::deque<float> pending_frames;
    std::thread render_thread;
    std::atomic<bool> stop_render_thread {false};
    std::uint16_t expected_sequence_number = 0;
    std::uint32_t expected_timestamp = 0;
    bool has_playout_cursor = false;
    bool playout_started = false;
    bool playout_wait_logged = false;
  };
}  // namespace platf::audio
