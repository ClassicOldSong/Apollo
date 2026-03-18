/**
 * @file src/platform/windows/apollo_vmic.h
 * @brief Steam Streaming Microphone backend definitions.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace platf::audio {
  class mic_write_wasapi_t;

  class mic_redirect_backend_t {
  public:
    virtual ~mic_redirect_backend_t() = default;

    virtual std::string_view backend_id() const = 0;
    virtual int init() = 0;
    virtual int write_data(const char *data, std::size_t len, std::uint16_t sequence_number, std::uint32_t timestamp) = 0;
  };

  class apollo_vmic_t final: public mic_redirect_backend_t {
  public:
    ~apollo_vmic_t() override;

    std::string_view backend_id() const override;
    int init() override;
    int write_data(const char *data, std::size_t len, std::uint16_t sequence_number, std::uint32_t timestamp) override;

  private:
    bool log_missing_driver_once();

    bool missing_driver_logged = false;
    std::unique_ptr<mic_write_wasapi_t> speaker_backend;
  };
}  // namespace platf::audio
