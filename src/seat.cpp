/**
 * @file src/seat.cpp
 * @brief Implementation of multi-seat management.
 */

// local includes
#include "seat.h"
#include "logging.h"

using namespace std::literals;

namespace seat {

  manager_t manager;

  seat_ptr make_default_seat() {
    auto seat = std::make_shared<seat_t>();
    seat->id = "default";
    // Empty display_name and audio_sink_id cause fallback to global config
    return seat;
  }

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

    seat->state = state_e::AVAILABLE;
    seat->bound_session.reset();

    // In single-seat mode, keep the default seat around for reuse.
    // In multi-seat mode (future), we may tear down virtual displays here.
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
