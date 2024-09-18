/**
 * @file src/nvhttp.h
 * @brief Declarations for the nvhttp (GameStream) server.
 */
// macros
#pragma once

// standard includes
#include <string>
#include <chrono>

// lib includes
#include <Simple-Web-Server/server_https.hpp>
#include <boost/property_tree/ptree.hpp>

// local includes
#include "crypto.h"
#include "rtsp.h"
#include "thread_safe.h"

using namespace std::chrono_literals;

/**
 * @brief Contains all the functions and variables related to the nvhttp (GameStream) server.
 */
namespace nvhttp {

  using args_t = SimpleWeb::CaseInsensitiveMultimap;

  /**
   * @brief The protocol version.
   * @details The version of the GameStream protocol we are mocking.
   * @note The negative 4th number indicates to Moonlight that this is Sunshine.
   */
  constexpr auto VERSION = "7.1.431.-1";

  /**
   * @brief The GFE version we are replicating.
   */
  constexpr auto GFE_VERSION = "3.23.0.74";

  /**
   * @brief The HTTP port, as a difference from the config port.
   */
  constexpr auto PORT_HTTP = 0;

  /**
   * @brief The HTTPS port, as a difference from the config port.
   */
  constexpr auto PORT_HTTPS = -5;

  constexpr auto OTP_EXPIRE_DURATION = 180s;

  /**
   * @brief Start the nvhttp server.
   * @examples
   * nvhttp::start();
   * @examples_end
   */
  void
  start();

  std::string
  get_arg(const args_t &args, const char *name, const char *default_value = nullptr);

  std::shared_ptr<rtsp_stream::launch_session_t>
  make_launch_session(bool host_audio, int appid, const args_t &args, const crypto::named_cert_t* named_cert_p);

  /**
   * @brief Compare the user supplied pin to the Moonlight pin.
   * @param pin The user supplied pin.
   * @param name The user supplied name.
   * @return `true` if the pin is correct, `false` otherwise.
   * @examples
   * bool pin_status = nvhttp::pin("1234", "laptop");
   * @examples_end
   */
  bool
  pin(std::string pin, std::string name);

  std::string request_otp(const std::string& passphrase, const std::string& deviceName);

  /**
   * @brief Remove single client.
   * @examples
   * nvhttp::unpair_client("4D7BB2DD-5704-A405-B41C-891A022932E1");
   * @examples_end
   */
  int
  unpair_client(std::string uniqueid);

  /**
   * @brief Get all paired clients.
   * @return The list of all paired clients.
   * @examples
   * boost::property_tree::ptree clients = nvhttp::get_all_clients();
   * @examples_end
   */
  boost::property_tree::ptree
  get_all_clients();

  /**
   * @brief Remove all paired clients.
   * @examples
   * nvhttp::erase_all_clients();
   * @examples_end
   */
  void
  erase_all_clients();

  /**
   * @brief      Stops a session.
   *
   * @param      session   The session
   * @param[in]  graceful  Whether to stop gracefully
   */
  void stop_session(stream::session_t& session, bool graceful);

  /**
   * @brief      Finds and stop session.
   *
   * @param[in]  uuid      The uuid string
   * @param[in]  graceful  Whether to stop gracefully
   */
  bool find_and_stop_session(const std::string& uuid, bool graceful);

  /**
   * @brief      Update device info associated to the session
   *
   * @param      session  The session
   * @param[in]  name     New name
   * @param[in]  newPerm  New permission
   */
  void update_session_info(stream::session_t& session, const std::string& name, const crypto::PERM newPerm);

  /**
   * @brief      Finds and udpate session information.
   *
   * @param[in]  uuid     The uuid string
   * @param[in]  name     New name
   * @param[in]  newPerm  New permission
   */
  bool find_and_udpate_session_info(const std::string& uuid, const std::string& name, const crypto::PERM newPerm);

  /**
   * @brief      Update device info
   *
   * @param[in]  uuid     The uuid string
   * @param[in]  name     New name
   * @param[in]  newPerm  New permission
   */
  bool update_device_info(const std::string& uuid, const std::string& name, const crypto::PERM newPerm);
}  // namespace nvhttp
