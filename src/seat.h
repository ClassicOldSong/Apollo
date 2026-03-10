/**
 * @file src/seat.h
 * @brief Declarations for multi-seat management.
 *
 * A "seat" is a bundle of desktop resources (display, audio endpoint, input target)
 * assigned to one streaming session. In single-seat mode (default), a single default
 * seat transparently wraps the existing global configuration. In multi-seat mode,
 * each concurrent session gets its own isolated seat with dedicated resources.
 */
#pragma once

// standard includes
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <guiddef.h>
#endif

namespace seat {

  enum class state_e {
    AVAILABLE,   ///< Seat resources are allocated but not bound to a session
    BOUND,       ///< Seat is actively streaming
    RELEASING,   ///< Seat is being torn down
  };

  /**
   * @brief Represents an input routing target for a seat.
   */
  struct input_target_t {
    std::string display_name;  ///< For SendInput targeting (Windows)
  };

  /**
   * @brief A bundle of desktop resources assigned to one streaming session.
   */
  struct seat_t {
    std::string id;                    ///< Unique seat identifier
    std::string display_name;          ///< Video capture target (empty = use config default)
    std::string audio_sink_id;         ///< Audio capture target (empty = system default)
    input_target_t input_target;

#ifdef _WIN32
    std::optional<GUID> vdisplay_guid; ///< Virtual display GUID (Windows)

    /**
     * @brief Create a virtual display for this seat using SudoVDA.
     * @param client_uid Client unique ID for display identity.
     * @param client_name Client display name.
     * @param width Display width in pixels.
     * @param height Display height in pixels.
     * @param fps Display refresh rate (in mHz, e.g., 60000 = 60Hz).
     * @param guid The GUID to assign to the virtual display.
     * @return true if the virtual display was created successfully.
     *
     * Sets display_name, vdisplay_guid, and input_target on success.
     * No-op if the seat already owns a virtual display.
     */
    bool setup_virtual_display(
      const std::string &client_uid,
      const std::string &client_name,
      uint32_t width,
      uint32_t height,
      uint32_t fps,
      const GUID &guid
    );

    /**
     * @brief Adopt an existing virtual display created elsewhere (e.g., by proc).
     * @param guid The virtual display GUID.
     * @param name The display device name (UTF-8).
     *
     * Transfers ownership of the virtual display to this seat so it will be
     * cleaned up when the seat is released.
     */
    void adopt_virtual_display(const GUID &guid, const std::string &name);

    /**
     * @brief Tear down the virtual display owned by this seat.
     *
     * Removes the virtual display via SudoVDA and clears vdisplay_guid.
     * Safe to call even if no virtual display is owned.
     */
    void teardown_virtual_display();
#endif

    state_e state = state_e::AVAILABLE;

    /**
     * @brief The session currently bound to this seat.
     * Weak pointer to avoid circular references with session_t.
     */
    std::weak_ptr<void> bound_session;
  };

  using seat_ptr = std::shared_ptr<seat_t>;

  /**
   * @brief Creates a default seat that mirrors current single-user behavior.
   *
   * The default seat uses empty display_name and audio_sink_id, which causes
   * all subsystems to fall back to their existing global configuration.
   */
  seat_ptr make_default_seat();

  /**
   * @brief Manages seat allocation, tracking, and release.
   *
   * In single-seat mode, acquire() always returns the same default seat.
   * In multi-seat mode, it allocates from a pool or creates new virtual displays.
   */
  class manager_t {
  public:
    /**
     * @brief Acquire a seat for a new session.
     * @return A seat pointer, or nullptr if no seat is available.
     */
    seat_ptr acquire();

    /**
     * @brief Release a seat when a session ends.
     * @param seat The seat to release.
     */
    void release(const seat_ptr &seat);

    /**
     * @brief Get all active (non-available) seats.
     * @return Vector of seat pointers.
     */
    std::vector<seat_ptr> active_seats() const;

    /**
     * @brief Check if multi-seating is enabled.
     * @return true if multi-seat mode is active.
     */
    bool multi_seat_enabled() const;

  private:
    mutable std::mutex _mutex;
    std::vector<seat_ptr> _seats;
    seat_ptr _default_seat;  ///< Singleton seat for single-seat mode
    bool _multi_seat = false;  ///< Driven by config (future Phase 8)
  };

  /**
   * @brief Global seat manager instance.
   */
  extern manager_t manager;

}  // namespace seat
