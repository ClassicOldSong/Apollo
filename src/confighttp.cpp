/**
 * @file src/confighttp.cpp
 * @brief Definitions for the Web UI Config HTTP server.
 *
 * @todo Authentication, better handling of routes common to nvhttp, cleanup
 */
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include "process.h"

#include <filesystem>
#include <set>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/asio/ssl/context.hpp>

#include <boost/filesystem.hpp>

#include <Simple-Web-Server/crypto.hpp>
#include <Simple-Web-Server/server_https.hpp>
#include <boost/asio/ssl/context_base.hpp>

#include "config.h"
#include "confighttp.h"
#include "crypto.h"
#include "display_device.h"
#include "file_handler.h"
#include "globals.h"
#include "httpcommon.h"
#include "logging.h"
#include "network.h"
#include "nvhttp.h"
#include "platform/common.h"
#include "rtsp.h"
#include "utility.h"
#include "uuid.h"
#include "version.h"
#include "process.h"

using namespace std::literals;

namespace confighttp {
  namespace fs = std::filesystem;
  namespace pt = boost::property_tree;

  using https_server_t = SimpleWeb::Server<SimpleWeb::HTTPS>;

  using args_t = SimpleWeb::CaseInsensitiveMultimap;
  using resp_https_t = std::shared_ptr<typename SimpleWeb::ServerBase<SimpleWeb::HTTPS>::Response>;
  using req_https_t = std::shared_ptr<typename SimpleWeb::ServerBase<SimpleWeb::HTTPS>::Request>;

  std::string sessionCookie;
  static std::chrono::time_point<std::chrono::steady_clock> cookie_creation_time;

  enum class op_e {
    ADD,  ///< Add client
    REMOVE  ///< Remove client
  };

  /**
   * @brief Log the request details.
   * @param request The HTTP request object.
   */
  void
  print_req(const req_https_t &request) {
    BOOST_LOG(debug) << "METHOD :: "sv << request->method;
    BOOST_LOG(debug) << "DESTINATION :: "sv << request->path;

    for (auto &[name, val] : request->header) {
      BOOST_LOG(debug) << name << " -- " << (name == "Authorization" ? "CREDENTIALS REDACTED" : val);
    }

    BOOST_LOG(debug) << " [--] "sv;

    for (auto &[name, val] : request->parse_query_string()) {
      BOOST_LOG(debug) << name << " -- " << val;
    }

    BOOST_LOG(debug) << " [--] "sv;
  }

  /**
   * @brief Send a response.
   * @param response The HTTP response object.
   * @param output_tree The JSON tree to send.
   */
  void
  send_response(resp_https_t response, const pt::ptree &output_tree) {
    std::ostringstream data;
    pt::write_json(data, output_tree);
    response->write(data.str());
  }

  /**
   * @brief Send a 401 Unauthorized response.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  send_unauthorized(resp_https_t response, req_https_t request) {
    auto address = net::addr_to_normalized_string(request->remote_endpoint().address());
    BOOST_LOG(info) << "Web UI: ["sv << address << "] -- not authorized"sv;
    response->write(SimpleWeb::StatusCode::client_error_unauthorized);
  }

  /**
   * @brief Send a redirect response.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * @param path The path to redirect to.
   */
  void
  send_redirect(resp_https_t response, req_https_t request, const char *path) {
    auto address = net::addr_to_normalized_string(request->remote_endpoint().address());
    BOOST_LOG(info) << "Web UI: ["sv << address << "] -- not authorized"sv;
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Location", path }
    };
    response->write(SimpleWeb::StatusCode::redirection_temporary_redirect, headers);
  }

  std::string getCookieValue(const std::string& cookieString, const std::string& key) {
    std::string keyWithEqual = key + "=";
    std::size_t startPos = cookieString.find(keyWithEqual);

    if (startPos == std::string::npos) {
      return "";
    }

    startPos += keyWithEqual.length();
    std::size_t endPos = cookieString.find(";", startPos);

    if (endPos == std::string::npos) {
      return cookieString.substr(startPos);
    }

    return cookieString.substr(startPos, endPos - startPos);
  }

  bool
  checkIPOrigin(resp_https_t response, req_https_t request) {
    auto address = net::addr_to_normalized_string(request->remote_endpoint().address());
    auto ip_type = net::from_address(address);

    if (ip_type > http::origin_web_ui_allowed) {
      BOOST_LOG(info) << "Web UI: ["sv << address << "] -- denied"sv;
      response->write(SimpleWeb::StatusCode::client_error_forbidden);
      return false;
    }

    return true;
  }

  bool
  authenticate(resp_https_t response, req_https_t request, bool needsRedirect = false) {
    if (!checkIPOrigin(response, request)) {
      return false;
    }

    // If credentials are shown, redirect the user to a /welcome page
    if (config::sunshine.username.empty()) {
      send_redirect(response, request, "/welcome");
      return false;
    }

    auto fg = util::fail_guard([&]() {
      if (needsRedirect) {
        std::string redir_path = "/login?redir=.";
        redir_path += request->path;
        send_redirect(response, request, redir_path.c_str());
      } else {
        send_unauthorized(response, request);
      }
    });

    if (sessionCookie.empty()) {
      return false;
    }

    // Cookie has expired
    if (std::chrono::steady_clock::now() - cookie_creation_time > SESSION_EXPIRE_DURATION) {
      sessionCookie.clear();
      return false;
    }

    auto cookies = request->header.find("cookie");
    if (cookies == request->header.end()) {
      return false;
    }

    auto authCookie = getCookieValue(cookies->second, "auth");
    if (authCookie.empty() || util::hex(crypto::hash(authCookie + config::sunshine.salt)).to_string() != sessionCookie) {
      return false;
    }

    fg.disable();
    return true;
  }

  /**
   * @brief Send a 404 Not Found response.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  not_found(resp_https_t response, [[maybe_unused]] req_https_t request) {
    constexpr SimpleWeb::StatusCode code = SimpleWeb::StatusCode::client_error_not_found;

    pt::ptree tree;
    tree.put("status_code", static_cast<int>(code));
    tree.put("error", "Not Found");

    std::ostringstream data;
    pt::write_json(data, tree);

    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");

    response->write(code, data.str(), headers);
  }

  /**
   * @brief Send a 400 Bad Request response.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * @param error_message The error message to include in the response.
   */
  void
  bad_request(resp_https_t response, [[maybe_unused]] req_https_t request, const std::string &error_message = "Bad Request") {
    constexpr SimpleWeb::StatusCode code = SimpleWeb::StatusCode::client_error_bad_request;

    pt::ptree tree;
    tree.put("status_code", static_cast<int>(code));
    tree.put("status", false);
    tree.put("error", error_message);

    std::ostringstream data;
    pt::write_json(data, tree);

    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");

    response->write(code, data.str(), headers);
  }

  /**
   * @brief Get the index page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * @todo combine these functions into a single function that accepts the page, i.e "index", "pin", "apps"
   */
  void
  fetchStaticPage(resp_https_t response, req_https_t request, const std::string& page, bool needsAuthenticate) {
    if (needsAuthenticate) {
      if (!authenticate(response, request, true)) return;
    }

    print_req(request);

    std::string content = file_handler::read_file((WEB_DIR + page).c_str());
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Content-Type", "text/html; charset=utf-8" },
      { "Access-Control-Allow-Origin", "https://images.igdb.com/"}
    };
    response->write(content, headers);
  };

  /**
   * @brief Get the PIN page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getIndexPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "index.html", true);
  }

  /**
   * @brief Get the apps page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getPinPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "pin.html", true);
  }

  /**
   * @brief Get the clients page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getAppsPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "apps.html", true);
  }

  /**
   * @brief Get the configuration page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getConfigPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "config.html", true);
  }

  /**
   * @brief Get the password page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getPasswordPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "password.html", true);
  }

  /**
   * @brief Get the welcome page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getWelcomePage(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    if (!config::sunshine.username.empty()) {
      send_redirect(response, request, "/");
      return;
    }

    fetchStaticPage(response, request, "welcome.html", false);
  }

  /**
   * @brief Get the troubleshooting page.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getLoginPage(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    if (config::sunshine.username.empty()) {
      send_redirect(response, request, "/welcome");
      return;
    }

    fetchStaticPage(response, request, "login.html", false);
  }

  void
  getTroubleshootingPage(resp_https_t response, req_https_t request) {
    fetchStaticPage(response, request, "troubleshooting.html", true);
  }

  /**
   * @brief Get the favicon image.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * @todo combine function with getSunshineLogoImage and possibly getNodeModules
   * @todo use mime_types map
   */
  void
  getFaviconImage(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    print_req(request);

    std::ifstream in(WEB_DIR "images/apollo.ico", std::ios::binary);
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Content-Type", "image/x-icon" }
    };
    response->write(SimpleWeb::StatusCode::success_ok, in, headers);
  }

  /**
   * @brief Get the Sunshine logo image.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * @todo combine function with getFaviconImage and possibly getNodeModules
   * @todo use mime_types map
   */
  void
  getSunshineLogoImage(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    print_req(request);

    std::ifstream in(WEB_DIR "images/logo-apollo-45.png", std::ios::binary);
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Content-Type", "image/png" }
    };
    response->write(SimpleWeb::StatusCode::success_ok, in, headers);
  }

  /**
   * @brief Check if a path is a child of another path.
   * @param base The base path.
   * @param query The path to check.
   * @return True if the path is a child of the base path, false otherwise.
   */
  bool
  isChildPath(fs::path const &base, fs::path const &query) {
    auto relPath = fs::relative(base, query);
    return *(relPath.begin()) != fs::path("..");
  }

  /**
   * @brief Get an asset from the node_modules directory.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   */
  void
  getNodeModules(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    print_req(request);
    fs::path webDirPath(WEB_DIR);
    fs::path nodeModulesPath(webDirPath / "assets");

    // .relative_path is needed to shed any leading slash that might exist in the request path
    auto filePath = fs::weakly_canonical(webDirPath / fs::path(request->path).relative_path());

    // Don't do anything if file does not exist or is outside the assets directory
    if (!isChildPath(filePath, nodeModulesPath)) {
      BOOST_LOG(warning) << "Someone requested a path " << filePath << " that is outside the assets folder";
      bad_request(response, request);
      return;
    }
    if (!fs::exists(filePath)) {
      not_found(response, request);
      return;
    }

    auto relPath = fs::relative(filePath, webDirPath);
    // get the mime type from the file extension mime_types map
    // remove the leading period from the extension
    auto mimeType = mime_types.find(relPath.extension().string().substr(1));
    // check if the extension is in the map at the x position
    if (mimeType == mime_types.end()) {
      bad_request(response, request);
      return;
    }

    // if it is, set the content type to the mime type
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", mimeType->second);
    std::ifstream in(filePath.string(), std::ios::binary);
    response->write(SimpleWeb::StatusCode::success_ok, in, headers);
  }

  /**
   * @brief Get the list of available applications.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/apps| GET| null}
   */
  void
  getApps(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::string content = file_handler::read_file(config::stream.file_apps.c_str());
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Content-Type", "application/json" }
    };
    response->write(content, headers);
  }

  /**
   * @brief Get the logs from the log file.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/logs| GET| null}
   */
  void
  getLogs(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::string content = file_handler::read_file(config::sunshine.log_file.c_str());
    const SimpleWeb::CaseInsensitiveMultimap headers {
      { "Content-Type", "text/plain" }
    };
    response->write(SimpleWeb::StatusCode::success_ok, content, headers);
  }

  /**
   * @brief Save an application. To save a new application the index must be `-1`. To update an existing application, you must provide the current index of the application.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *   "name": "<Application Name>",
   *   "output": "<Log Output Path>",
   *   "cmd": "<Command to run the application>",
   *   "index": -1,
   *   "exclude-global-prep-cmd": false,
   *   "elevated": false,
   *   "auto-detach": true,
   *   "wait-all": true,
   *   "exit-timeout": 5,
   *   "prep-cmd": [
   *     {
   *       "do": "<Command to prepare>",
   *       "undo": "<Command to undo preparation>",
   *       "elevated": false
   *     }
   *   ],
   *   "detached": [
   *     "<Detached commands>"
   *   ],
   *   "image-path": "<Full path to the application image. Must be a png file.>",
   *   "uuid": "C3445C24-871A-FD23-0708-615C121B5B78"
   * }
   * @endcode
   *
   * @api_examples{/api/apps| POST| {"name":"Hello, World!","index":-1}}
   */
  void
  saveApp(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    ss << request->content.rdbuf();

    BOOST_LOG(info) << config::stream.file_apps;
    try {
      // TODO: Input Validation
      pt::ptree fileTree;
      pt::ptree inputTree;
      pt::ptree outputTree;
      pt::read_json(ss, inputTree);
      pt::read_json(config::stream.file_apps, fileTree);

      proc::migrate_apps(&fileTree, &inputTree);

      pt::write_json(config::stream.file_apps, fileTree);
      proc::refresh(config::stream.file_apps);

      outputTree.put("status", true);
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "SaveApp: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  /**
   * @brief Delete an application.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/apps/9999| DELETE| null}
   */
  void
  deleteApp(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    auto args = request->parse_query_string();
    if (
      args.find("uuid"s) == std::end(args)
    ) {
      bad_request(response, request, "Missing a required parameter to delete app");
      return;
    }

    auto uuid = nvhttp::get_arg(args, "uuid");

    pt::ptree fileTree;
    try {
      pt::read_json(config::stream.file_apps, fileTree);
      auto &apps_node = fileTree.get_child("apps"s);

      // Unfortunately Boost PT does not allow to directly edit the array, copy should do the trick
      pt::ptree newApps;
      for (const auto &kv : apps_node) {
        auto app_uuid = kv.second.get_optional<std::string>("uuid"s);
        if (!app_uuid || app_uuid.value() != uuid) {
          newApps.push_back(std::make_pair("", kv.second));
        }
      }
      fileTree.erase("apps");
      fileTree.push_back(std::make_pair("apps", newApps));

      pt::write_json(config::stream.file_apps, fileTree);

      pt::ptree outputTree;
      outputTree.put("status", true);
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "DeleteApp: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  /**
   * @brief Upload a cover image.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *   "key": "igdb_<game_id>",
   *   "url": "https://images.igdb.com/igdb/image/upload/t_cover_big_2x/<slug>.png"
   * }
   * @endcode
   *
   * @api_examples{/api/covers/upload| POST| {"key":"igdb_1234","url":"https://images.igdb.com/igdb/image/upload/t_cover_big_2x/abc123.png"}}
   */
  void
  uploadCover(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    std::stringstream ss;
    std::stringstream configStream;
    ss << request->content.rdbuf();
    pt::ptree outputTree;
    pt::ptree inputTree;
    try {
      pt::read_json(ss, inputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "UploadCover: "sv << e.what();
      bad_request(response, request, e.what());
      return;
    }

    auto key = inputTree.get("key", "");
    if (key.empty()) {
      bad_request(response, request, "Cover key is required");
      return;
    }
    auto url = inputTree.get("url", "");

    const std::string coverdir = platf::appdata().string() + "/covers/";
    file_handler::make_directory(coverdir);

    std::basic_string path = coverdir + http::url_escape(key) + ".png";
    if (!url.empty()) {
      if (http::url_get_host(url) != "images.igdb.com") {
        bad_request(response, request, "Only images.igdb.com is allowed");
        return;
      }
      if (!http::download_file(url, path)) {
        bad_request(response, request, "Failed to download cover");
        return;
      }
    }
    else {
      auto data = SimpleWeb::Crypto::Base64::decode(inputTree.get<std::string>("data"));

      std::ofstream imgfile(path);
      imgfile.write(data.data(), (int) data.size());
    }
    outputTree.put("status", true);
    outputTree.put("path", path);
    send_response(response, outputTree);
  }

  /**
   * @brief Get the configuration settings.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/config| GET| null}
   */
  void
  getConfig(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    pt::ptree outputTree;
    outputTree.put("status", true);
    outputTree.put("platform", SUNSHINE_PLATFORM);
    outputTree.put("version", PROJECT_VER);
  #ifdef _WIN32
    outputTree.put("vdisplayStatus", (int)proc::vDisplayDriverStatus);
  #endif

    auto vars = config::parse_config(file_handler::read_file(config::sunshine.config_file.c_str()));

    for (auto &[name, value] : vars) {
      outputTree.put(std::move(name), std::move(value));
    }

    send_response(response, outputTree);
  }

  /**
   * @brief Get the locale setting. This endpoint does not require authentication.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/configLocale| GET| null}
   */
  void
  getLocale(resp_https_t response, req_https_t request) {
    // we need to return the locale whether authenticated or not

    print_req(request);

    pt::ptree outputTree;
    outputTree.put("status", true);
    outputTree.put("locale", config::sunshine.locale);
    send_response(response, outputTree);
  }

  /**
   * @brief Save the configuration settings.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *   "key": "value"
   * }
   * @endcode
   *
   * @attention{It is recommended to ONLY save the config settings that differ from the default behavior.}
   *
   * @api_examples{/api/config| POST| {"key":"value"}}
   */
  void
  saveConfig(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    std::stringstream configStream;
    ss << request->content.rdbuf();
    try {
      // TODO: Input Validation
      pt::ptree inputTree;
      pt::ptree outputTree;
      pt::read_json(ss, inputTree);
      for (const auto &kv : inputTree) {
        std::string value = inputTree.get<std::string>(kv.first);
        if (value.length() == 0 || value.compare("null") == 0) continue;

        configStream << kv.first << " = " << value << std::endl;
      }
      file_handler::write_file(config::sunshine.config_file.c_str(), configStream.str());
      outputTree.put("status", true);
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "SaveConfig: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  /**
   * @brief Restart Sunshine.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/restart| POST| null}
   */
  void
  restart(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    // We may not return from this call
    platf::restart();
  }

  void
  quit(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    BOOST_LOG(warning) << "Requested quit from config page!"sv;

  #ifdef _WIN32
    // If we're running in a service, return a special status to
    // tell it to terminate too, otherwise it will just respawn us.
    if (GetConsoleWindow() == NULL) {
      lifetime::exit_sunshine(ERROR_SHUTDOWN_IN_PROGRESS, true);
    } else
  #endif
    {
      lifetime::exit_sunshine(0, true);
    }

    // We do want to return here
    // If user get a return, then the exit has failed.
    // This might not be thread safe but we're exiting anyways
    std::thread write_resp([response]{
      std::this_thread::sleep_for(5s);
      response->write();
    });
    write_resp.detach();
  }

  /**
   * @brief Reset the display device persistence.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/reset-display-device-persistence| POST| null}
   */
  void
  resetDisplayDevicePersistence(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    pt::ptree outputTree;
    outputTree.put("status", display_device::reset_persistence());
    send_response(response, outputTree);
  }

  /**
   * @brief Update existing credentials.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *   "currentUsername": "Current Username",
   *   "currentPassword": "Current Password",
   *   "newUsername": "New Username",
   *   "newPassword": "New Password",
   *   "confirmNewPassword": "Confirm New Password"
   * }
   * @endcode
   *
   * @api_examples{/api/password| POST| {"currentUsername":"admin","currentPassword":"admin","newUsername":"admin","newPassword":"admin","confirmNewPassword":"admin"}}
   */
  void
  savePassword(resp_https_t response, req_https_t request) {
    if (!config::sunshine.username.empty() && !authenticate(response, request)) return;

    print_req(request);

    std::vector<std::string> errors = {};
    std::stringstream ss;
    std::stringstream configStream;
    ss << request->content.rdbuf();

    try {
      // TODO: Input Validation
      pt::ptree inputTree;
      pt::ptree outputTree;
      pt::read_json(ss, inputTree);
      auto username = inputTree.count("currentUsername") > 0 ? inputTree.get<std::string>("currentUsername") : "";
      auto newUsername = inputTree.get<std::string>("newUsername");
      auto password = inputTree.count("currentPassword") > 0 ? inputTree.get<std::string>("currentPassword") : "";
      auto newPassword = inputTree.count("newPassword") > 0 ? inputTree.get<std::string>("newPassword") : "";
      auto confirmPassword = inputTree.count("confirmNewPassword") > 0 ? inputTree.get<std::string>("confirmNewPassword") : "";
      if (newUsername.length() == 0) newUsername = username;
      if (newUsername.length() == 0) {
        errors.emplace_back("Invalid Username");
      }
      else {
        auto hash = util::hex(crypto::hash(password + config::sunshine.salt)).to_string();
        if (config::sunshine.username.empty() || (boost::iequals(username, config::sunshine.username) && hash == config::sunshine.password)) {
          if (newPassword.empty() || newPassword != confirmPassword) {
            errors.emplace_back("Password Mismatch");
          }
          else {
            http::save_user_creds(config::sunshine.credentials_file, newUsername, newPassword);
            http::reload_user_creds(config::sunshine.credentials_file);

            // Force user to re-login
            sessionCookie.clear();

            outputTree.put("status", true);
          }
        }
        else {
          errors.emplace_back("Invalid Current Credentials");
        }
      }

      if (!errors.empty()) {
        // join the errors array
        std::string error = std::accumulate(errors.begin(), errors.end(), std::string(),
          [](const std::string &a, const std::string &b) {
            return a.empty() ? b : a + ", " + b;
          });
        bad_request(response, request, error);
        return;
      }

      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "SavePassword: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  void
  login(resp_https_t response, req_https_t request) {
    if (!checkIPOrigin(response, request)) {
      return;
    }

    auto fg = util::fail_guard([&]{
      response->write(SimpleWeb::StatusCode::client_error_unauthorized);
    });

    std::stringstream ss;
    ss << request->content.rdbuf();

    pt::ptree inputTree;

    try {
      pt::read_json(ss, inputTree);
      std::string username = inputTree.get<std::string>("username");
      std::string password = inputTree.get<std::string>("password");
      std::string hash = util::hex(crypto::hash(password + config::sunshine.salt)).to_string();

      if (!boost::iequals(username, config::sunshine.username) || hash != config::sunshine.password) {
        return;
      }

      std::string sessionCookieRaw = crypto::rand_alphabet(64);
      sessionCookie = util::hex(crypto::hash(sessionCookieRaw + config::sunshine.salt)).to_string();
      cookie_creation_time = std::chrono::steady_clock::now();

      const SimpleWeb::CaseInsensitiveMultimap headers {
        { "Set-Cookie", "auth=" + sessionCookieRaw + "; Secure; Max-Age=2592000; Path=/" }
      };

      response->write(headers);

      fg.disable();
    } catch (std::exception &e) {
      BOOST_LOG(warning) << "Web UI Login failed: ["sv << net::addr_to_normalized_string(request->remote_endpoint().address()) << "]: "sv << e.what();
      response->write(SimpleWeb::StatusCode::server_error_internal_server_error);
      fg.disable();
      return;
    }
  }

  /**
   * @brief Send a pin code to the host. The pin is generated from the Moonlight client during the pairing process.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *   "pin": "<pin>",
   *   "name": "Friendly Client Name"
   * }
   * @endcode
   *
   * @api_examples{/api/pin| POST| {"pin":"1234","name":"My PC"}}
   */
  void
  savePin(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    ss << request->content.rdbuf();

    try {
      // TODO: Input Validation
      pt::ptree inputTree;
      pt::ptree outputTree;
      pt::read_json(ss, inputTree);
      std::string pin = inputTree.get<std::string>("pin");
      std::string name = inputTree.get<std::string>("name");
      outputTree.put("status", nvhttp::pin(pin, name));
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "SavePin: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  void
  getOTP(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    pt::ptree outputTree;

    try {
      auto args = request->parse_query_string();
      auto it = args.find("passphrase");
      if (it == std::end(args)) {
        throw std::runtime_error("Passphrase not provided!");
      }

      if (it->second.size() < 4) {
        throw std::runtime_error("Passphrase too short!");
      }

      std::string passphrase = it->second;
      std::string deviceName;

      it = args.find("deviceName");
      if (it != std::end(args)) {
        deviceName = it->second;
      }

      outputTree.put("otp", nvhttp::request_otp(passphrase, deviceName));
      outputTree.put("ip", platf::get_local_ip_for_gateway());
      outputTree.put("name", config::nvhttp.sunshine_name);
      outputTree.put("status", true);
      outputTree.put("message", "OTP created, effective within 3 minutes.");
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "OTP creation failed: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  void
  updateClient(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    ss << request->content.rdbuf();

    pt::ptree inputTree, outputTree;

    try {
      pt::read_json(ss, inputTree);
      std::string uuid = inputTree.get<std::string>("uuid");
      std::string name = inputTree.get<std::string>("name");
      auto perm = (crypto::PERM)inputTree.get<uint32_t>("perm") & crypto::PERM::_all;
      outputTree.put("status", nvhttp::update_device_info(uuid, name, perm));
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "Update Client: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  /**
   * @brief Unpair all clients.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/clients/unpair-all| POST| null}
   */
  void
  unpairAll(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    nvhttp::erase_all_clients();
    proc::proc.terminate();

    pt::ptree outputTree;
    outputTree.put("status", true);
    send_response(response, outputTree);
  }

  /**
   * @brief Unpair a client.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   * The body for the post request should be JSON serialized in the following format:
   * @code{.json}
   * {
   *  "uuid": "<uuid>"
   * }
   * @endcode
   *
   * @api_examples{/api/unpair| POST| {"uuid":"1234"}}
   */
  void
  unpair(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    ss << request->content.rdbuf();

    try {
      // TODO: Input Validation
      pt::ptree inputTree;
      pt::ptree outputTree;
      pt::read_json(ss, inputTree);
      std::string uuid = inputTree.get<std::string>("uuid");
      outputTree.put("status", nvhttp::unpair_client(uuid));
      send_response(response, outputTree);
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "Unpair: "sv << e.what();
      bad_request(response, request, e.what());
    }
  }

  void launchApp(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    pt::ptree outputTree;

    auto args = request->parse_query_string();
    if (
      args.find("uuid"s) == std::end(args)
    ) {
      bad_request(response, request, "Missing a required launch parameter");
      return;
    }

    auto uuid = nvhttp::get_arg(args, "uuid");

    const auto& apps = proc::proc.get_apps();

    for (auto& app : apps) {
      if (app.uuid == uuid) {
        auto appid = util::from_view(app.id);

        crypto::named_cert_t named_cert {
          .name = "",
          .uuid = http::unique_id,
          .perm = crypto::PERM::_all,
        };

        BOOST_LOG(info) << "Launching app ["sv << app.name << "] from web UI"sv;

        auto launch_session = nvhttp::make_launch_session(true, appid, args, &named_cert);
        auto err = proc::proc.execute(appid, app, launch_session);
        if (err) {
          bad_request(response, request, err == 503
            ? "Failed to initialize video capture/encoding. Is a display connected and turned on?"
            : "Failed to start the specified application");
        } else {
          outputTree.put("status", true);
          send_response(response, outputTree);
        }

        return;
      }
    }

    BOOST_LOG(error) << "Couldn't find app with uuid ["sv << uuid << ']';
    bad_request(response, request, "Cannot find requested application");
  }

  void
  disconnect(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    std::stringstream ss;
    ss << request->content.rdbuf();

    pt::ptree inputTree, outputTree;

    try {
      pt::read_json(ss, inputTree);
      std::string uuid = inputTree.get<std::string>("uuid");
      outputTree.put("status", nvhttp::find_and_stop_session(uuid, true));
    }
    catch (std::exception &e) {
      BOOST_LOG(warning) << "Disconnect: "sv << e.what();
      bad_request(response, request, e.what());
    }
    send_response(response, outputTree);
  }

  /**
   * @brief Get the list of paired clients.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/clients/list| GET| null}
   */
  void
  listClients(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    const pt::ptree named_certs = nvhttp::get_all_clients();

    pt::ptree outputTree;
    outputTree.put("status", false);
    outputTree.add_child("named_certs", named_certs);
    outputTree.put("status", true);
    send_response(response, outputTree);
  }

  /**
   * @brief Close the currently running application.
   * @param response The HTTP response object.
   * @param request The HTTP request object.
   *
   * @api_examples{/api/apps/close| POST| null}
   */
  void
  closeApp(resp_https_t response, req_https_t request) {
    if (!authenticate(response, request)) return;

    print_req(request);

    proc::proc.terminate();

    pt::ptree outputTree;
    outputTree.put("status", true);
    send_response(response, outputTree);
  }

  void
  start() {
    auto shutdown_event = mail::man->event<bool>(mail::shutdown);

    auto port_https = net::map_port(PORT_HTTPS);
    auto address_family = net::af_from_enum_string(config::sunshine.address_family);

    https_server_t server { config::nvhttp.cert, config::nvhttp.pkey };
    server.default_resource["DELETE"] = [](resp_https_t response, req_https_t request) {
      bad_request(response, request);
    };
    server.default_resource["PATCH"] = [](resp_https_t response, req_https_t request) {
      bad_request(response, request);
    };
    server.default_resource["POST"] = [](resp_https_t response, req_https_t request) {
      bad_request(response, request);
    };
    server.default_resource["PUT"] = [](resp_https_t response, req_https_t request) {
      bad_request(response, request);
    };
    server.default_resource["GET"] = not_found;
    server.resource["^/$"]["GET"] = getIndexPage;
    server.resource["^/pin/?$"]["GET"] = getPinPage;
    server.resource["^/apps/?$"]["GET"] = getAppsPage;
    server.resource["^/config/?$"]["GET"] = getConfigPage;
    server.resource["^/password/?$"]["GET"] = getPasswordPage;
    server.resource["^/welcome/?$"]["GET"] = getWelcomePage;
    server.resource["^/login/?$"]["GET"] = getLoginPage;
    server.resource["^/troubleshooting/?$"]["GET"] = getTroubleshootingPage;
    server.resource["^/api/login"]["POST"] = login;
    server.resource["^/api/pin$"]["POST"] = savePin;
    server.resource["^/api/otp$"]["GET"] = getOTP;
    server.resource["^/api/apps$"]["GET"] = getApps;
    server.resource["^/api/apps$"]["POST"] = saveApp;
    server.resource["^/api/apps/delete$"]["POST"] = deleteApp;
    server.resource["^/api/apps/launch$"]["POST"] = launchApp;
    server.resource["^/api/apps/close$"]["POST"] = closeApp;
    server.resource["^/api/logs$"]["GET"] = getLogs;
    server.resource["^/api/config$"]["GET"] = getConfig;
    server.resource["^/api/config$"]["POST"] = saveConfig;
    server.resource["^/api/configLocale$"]["GET"] = getLocale;
    server.resource["^/api/restart$"]["POST"] = restart;
    server.resource["^/api/quit$"]["POST"] = quit;
    server.resource["^/api/reset-display-device-persistence$"]["POST"] = resetDisplayDevicePersistence;
    server.resource["^/api/password$"]["POST"] = savePassword;
    server.resource["^/api/clients/unpair-all$"]["POST"] = unpairAll;
    server.resource["^/api/clients/list$"]["GET"] = listClients;
    server.resource["^/api/clients/update$"]["POST"] = updateClient;
    server.resource["^/api/clients/unpair$"]["POST"] = unpair;
    server.resource["^/api/clients/disconnect$"]["POST"] = disconnect;
    server.resource["^/api/covers/upload$"]["POST"] = uploadCover;
    server.resource["^/images/apollo.ico$"]["GET"] = getFaviconImage;
    server.resource["^/images/logo-apollo-45.png$"]["GET"] = getSunshineLogoImage;
    server.resource["^/assets\\/.+$"]["GET"] = getNodeModules;
    server.config.reuse_address = true;
    server.config.address = net::af_to_any_address_string(address_family);
    server.config.port = port_https;

    auto accept_and_run = [&](auto *server) {
      try {
        server->start([](unsigned short port) {
          BOOST_LOG(info) << "Configuration UI available at [https://localhost:"sv << port << "]";
        });
      }
      catch (boost::system::system_error &err) {
        // It's possible the exception gets thrown after calling server->stop() from a different thread
        if (shutdown_event->peek()) {
          return;
        }

        BOOST_LOG(fatal) << "Couldn't start Configuration HTTPS server on port ["sv << port_https << "]: "sv << err.what();
        shutdown_event->raise(true);
        return;
      }
    };
    std::thread tcp { accept_and_run, &server };

    // Wait for any event
    shutdown_event->view();

    server.stop();

    tcp.join();
  }
}  // namespace confighttp
