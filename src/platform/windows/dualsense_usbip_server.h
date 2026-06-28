/**
 * @file src/platform/windows/dualsense_usbip_server.h
 * @brief Windows USB/IP server for a virtual DualSense Edge controller.
 */
#pragma once

// standard includes
#include <cstdint>
#include <functional>
#include <memory>

// local includes
#include "src/platform/common.h"
#include "src/platform/dualsense.h"

namespace platf::dualsense_usbip {
  class server_t {
  public:
    using output_cb_t = std::function<void(const dualsense::output_report_state_t &report)>;

    server_t();
    ~server_t();

    server_t(const server_t &) = delete;
    server_t &operator=(const server_t &) = delete;

    bool start(output_cb_t output_cb, std::uint16_t port = 3240);
    void stop();

    bool is_running() const;
    bool is_attached() const;

    void update_input(const gamepad_state_t &state);

  private:
    struct impl_t;
    std::unique_ptr<impl_t> impl_;
  };
}  // namespace platf::dualsense_usbip
