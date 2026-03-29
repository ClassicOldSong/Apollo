/**
 * @file src/platform/windows/apollo_vmic.cpp
 * @brief Steam Streaming Microphone backend for Windows host-side mic injection.
 */
#include "apollo_vmic.h"

#include "mic_write.h"
#include "src/logging.h"

namespace platf::audio {
  apollo_vmic_t::~apollo_vmic_t() = default;

  std::string_view apollo_vmic_t::backend_id() const {
    return "steam_streaming_microphone";
  }

  bool apollo_vmic_t::log_missing_driver_once() {
    if (missing_driver_logged) {
      return false;
    }

    missing_driver_logged = true;
    BOOST_LOG(warning)
      << "Steam Streaming Microphone is unavailable. Install the local Steam audio drivers and ensure the "
      << "\"Speakers (Steam Streaming Microphone)\" playback endpoint is present. Host applications should capture from "
      << "\"Microphone (Steam Streaming Microphone)\".";
    return true;
  }

  int apollo_vmic_t::init() {
    if (!speaker_backend) {
      speaker_backend = std::make_unique<mic_write_wasapi_t>(
        "steam_streaming_microphone",
        std::vector<std::wstring> {
          L"Steam Streaming Microphone",
          L"Speakers (Steam Streaming Microphone)",
        }
      );
    }

    if (speaker_backend->init() == 0) {
      return 0;
    }

    speaker_backend.reset();
    log_missing_driver_once();
    return -1;
  }

  int apollo_vmic_t::write_data(const char *data, std::size_t len, std::uint16_t sequence_number, std::uint32_t timestamp) {
    if (!speaker_backend) {
      BOOST_LOG(warning) << "Client microphone packet rejected before decode because the Steam Streaming Microphone backend is missing"
                         << " [seq=" << sequence_number << ", ts=" << timestamp << ", len=" << len << ']';
      log_missing_driver_once();
      return -1;
    }

    return speaker_backend->write_data(data, len, sequence_number, timestamp);
  }
}  // namespace platf::audio
