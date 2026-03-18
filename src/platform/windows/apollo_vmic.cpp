/**
 * @file src/platform/windows/apollo_vmic.cpp
 * @brief VB-CABLE backend for Windows host-side mic injection.
 */
#include "apollo_vmic.h"

#include "mic_write.h"
#include "src/logging.h"

namespace platf::audio {
  apollo_vmic_t::~apollo_vmic_t() = default;

  std::string_view apollo_vmic_t::backend_id() const {
    return "vb_cable";
  }

  bool apollo_vmic_t::log_missing_driver_once() {
    if (missing_driver_logged) {
      return false;
    }

    missing_driver_logged = true;
    BOOST_LOG(warning)
      << "VB-CABLE microphone backend is unavailable. Install VB-CABLE and ensure the "
      << "\"CABLE Input\" playback endpoint is present. Host applications should capture from "
      << "\"CABLE Output\".";
    return true;
  }

  int apollo_vmic_t::init() {
    if (!speaker_backend) {
      speaker_backend = std::make_unique<mic_write_wasapi_t>(
        "vb_cable",
        std::vector<std::wstring> {
          L"CABLE Input",
          L"VB-Audio Virtual Cable",
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

  int apollo_vmic_t::write_data(const char *data, std::size_t len, std::uint16_t sequence_number) {
    if (!speaker_backend) {
      BOOST_LOG(warning) << "Client microphone packet rejected before decode because the VB-CABLE speaker backend is missing"
                         << " [seq=" << sequence_number << ", len=" << len << ']';
      log_missing_driver_once();
      return -1;
    }

    return speaker_backend->write_data(data, len, sequence_number);
  }
}  // namespace platf::audio
