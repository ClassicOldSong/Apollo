/**
 * @file src/platform/windows/mic_write.h
 * @brief Windows microphone redirection writer.
 */
#pragma once

#include <memory>
#include <string>
#include <string_view>
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
    mic_write_wasapi_t(std::string backend_name = "steam_streaming_microphone",
                       std::vector<std::wstring> autodetect_patterns = {},
                       std::string requested_device_name = {});
    ~mic_write_wasapi_t();

    std::string_view backend_id() const override;
    int init() override;
    int write_data(const char *data, std::size_t len, std::uint16_t sequence_number) override;
    void cleanup();

  private:
    bool initialize_device();
    bool find_target_device(EDataFlow flow, std::wstring &device_id, std::string &device_name);

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
  };
}  // namespace platf::audio
