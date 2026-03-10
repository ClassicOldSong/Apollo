/**
 * @file src/seat.cpp
 * @brief Implementation of multi-seat management.
 */

// local includes
#include "seat.h"
#include "logging.h"

#ifdef _WIN32
  #include "platform/windows/virtual_display.h"
  #include "platform/common.h"
#endif

using namespace std::literals;

namespace seat {

  manager_t manager;

  seat_ptr make_default_seat() {
    auto seat = std::make_shared<seat_t>();
    seat->id = "default";
    // Empty display_name and audio_sink_id cause fallback to global config
    return seat;
  }

#ifdef _WIN32
  bool seat_t::setup_virtual_display(
    const std::string &client_uid,
    const std::string &client_name,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    const GUID &guid
  ) {
    if (vdisplay_guid) {
      BOOST_LOG(info) << "Seat "sv << id << " already owns a virtual display"sv;
      return true;
    }

    auto vdisplay_name = VDISPLAY::createVirtualDisplay(
      client_uid.c_str(),
      client_name.c_str(),
      width,
      height,
      fps,
      guid
    );

    if (vdisplay_name.empty()) {
      BOOST_LOG(warning) << "Failed to create virtual display for seat "sv << id;
      return false;
    }

    vdisplay_guid = guid;
    display_name = platf::to_utf8(vdisplay_name);
    input_target.display_name = display_name;

    BOOST_LOG(info) << "Virtual display created for seat "sv << id << ": "sv << display_name;

    // Apply display settings
    if (width && height && fps) {
      VDISPLAY::changeDisplaySettings(vdisplay_name.c_str(), width, height, fps);
    }

    return true;
  }

  void seat_t::adopt_virtual_display(const GUID &guid, const std::string &name) {
    vdisplay_guid = guid;
    display_name = name;
    input_target.display_name = name;

    BOOST_LOG(info) << "Seat "sv << id << " adopted virtual display: "sv << name;
  }

  void seat_t::teardown_virtual_display() {
    if (!vdisplay_guid) {
      return;
    }

    if (VDISPLAY::removeVirtualDisplay(*vdisplay_guid)) {
      BOOST_LOG(info) << "Virtual display removed for seat "sv << id;
    } else {
      BOOST_LOG(warning) << "Failed to remove virtual display for seat "sv << id;
    }

    vdisplay_guid.reset();
    display_name.clear();
    input_target.display_name.clear();
  }
#endif

  seat_ptr manager_t::acquire() {
    std::lock_guard lg(_mutex);

    if (!_multi_seat) {
      // Single-seat mode: always return the same default seat
      if (!_default_seat) {
        _default_seat = make_default_seat();
      }

      _default_seat->state = state_e::BOUND;

      BOOST_LOG(info) << "Seat acquired: "sv << _default_seat->id;
      return _default_seat;
    }

    // Multi-seat mode: find an available seat or create one
    for (auto &s : _seats) {
      if (s->state == state_e::AVAILABLE) {
        s->state = state_e::BOUND;
        BOOST_LOG(info) << "Seat acquired: "sv << s->id;
        return s;
      }
    }

    // TODO (Phase 5+): Create a new seat with virtual display
    BOOST_LOG(warning) << "No available seats"sv;
    return nullptr;
  }

  void manager_t::release(const seat_ptr &seat) {
    if (!seat) return;

    std::lock_guard lg(_mutex);

    BOOST_LOG(info) << "Seat released: "sv << seat->id;

#ifdef _WIN32
    // Clean up seat-owned virtual display before releasing
    seat->teardown_virtual_display();
#endif

    seat->state = state_e::AVAILABLE;
    seat->bound_session.reset();

    // In single-seat mode, keep the default seat around for reuse.
    // In multi-seat mode, virtual displays are torn down above.
  }

  std::vector<seat_ptr> manager_t::active_seats() const {
    std::lock_guard lg(_mutex);

    std::vector<seat_ptr> result;

    if (!_multi_seat) {
      if (_default_seat && _default_seat->state == state_e::BOUND) {
        result.push_back(_default_seat);
      }
      return result;
    }

    for (const auto &s : _seats) {
      if (s->state == state_e::BOUND) {
        result.push_back(s);
      }
    }
    return result;
  }

  bool manager_t::multi_seat_enabled() const {
    std::lock_guard lg(_mutex);
    return _multi_seat;
  }

}  // namespace seat
