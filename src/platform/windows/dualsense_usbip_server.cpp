/**
 * @file src/platform/windows/dualsense_usbip_server.cpp
 * @brief Windows USB/IP server for a virtual DualSense Edge controller.
 */
// platform includes
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// standard includes
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

// local includes
#include "dualsense_usbip_server.h"
#include "src/logging.h"
#include "src/platform/dualsense_usbip.h"

namespace platf::dualsense_usbip {
  using namespace std::literals;

  namespace {
    constexpr SOCKET INVALID_SERVER_SOCKET = INVALID_SOCKET;
    constexpr auto INPUT_REPLY_INTERVAL = std::chrono::milliseconds {dualsense::DUALSENSE_EDGE_HID_INTERRUPT_IN_INTERVAL_MS};

    bool send_all(SOCKET socket, std::span<const std::uint8_t> data) {
      std::size_t sent = 0;
      while (sent < data.size()) {
        const auto chunk = static_cast<int>(std::min<std::size_t>(data.size() - sent, static_cast<std::size_t>(INT_MAX)));
        const auto result = ::send(socket, reinterpret_cast<const char *>(data.data() + sent), chunk, 0);
        if (result <= 0) {
          return false;
        }

        sent += static_cast<std::size_t>(result);
      }

      return true;
    }

    bool recv_exact(SOCKET socket, std::span<std::uint8_t> data) {
      std::size_t received = 0;
      while (received < data.size()) {
        const auto chunk = static_cast<int>(std::min<std::size_t>(data.size() - received, static_cast<std::size_t>(INT_MAX)));
        const auto result = ::recv(socket, reinterpret_cast<char *>(data.data() + received), chunk, 0);
        if (result <= 0) {
          return false;
        }

        received += static_cast<std::size_t>(result);
      }

      return true;
    }
  }  // namespace

  struct server_t::impl_t {
    ~impl_t() {
      stop();
    }

    bool start(server_t::output_cb_t cb, std::uint16_t port) {
      if (running.load()) {
        return true;
      }

      output_cb = std::move(cb);

      WSADATA wsa {};
      const auto wsa_status = ::WSAStartup(MAKEWORD(2, 2), &wsa);
      if (wsa_status != 0) {
        BOOST_LOG(error) << "DualSense Edge USB/IP: WSAStartup failed ["sv << wsa_status << ']';
        return false;
      }
      wsa_started = true;

      SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (socket == INVALID_SOCKET) {
        BOOST_LOG(error) << "DualSense Edge USB/IP: socket() failed ["sv << ::WSAGetLastError() << ']';
        cleanup_winsock();
        return false;
      }

      int reuse = 1;
      ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));

      sockaddr_in address {};
      address.sin_family = AF_INET;
      address.sin_port = htons(port);
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      if (::bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        BOOST_LOG(error) << "DualSense Edge USB/IP: bind failed on 127.0.0.1:"sv << port
                         << " ["sv << ::WSAGetLastError() << ']';
        ::closesocket(socket);
        cleanup_winsock();
        return false;
      }

      if (::listen(socket, 2) != 0) {
        BOOST_LOG(error) << "DualSense Edge USB/IP: listen failed on 127.0.0.1:"sv << port
                         << " ["sv << ::WSAGetLastError() << ']';
        ::closesocket(socket);
        cleanup_winsock();
        return false;
      }

      listen_socket.store(socket);
      stop_requested.store(false);
      running.store(true);
      accept_thread = std::thread(&impl_t::accept_loop, this);

      BOOST_LOG(info) << "DualSense Edge USB/IP: listening on 127.0.0.1:"sv << port;
      return true;
    }

    void stop() {
      if (!running.exchange(false)) {
        cleanup_winsock();
        return;
      }

      stop_requested.store(true);

      const auto listener = listen_socket.exchange(INVALID_SERVER_SOCKET);
      if (listener != INVALID_SERVER_SOCKET) {
        ::closesocket(listener);
      }

      const auto client = active_client.exchange(INVALID_SERVER_SOCKET);
      if (client != INVALID_SERVER_SOCKET) {
        ::closesocket(client);
      }

      if (accept_thread.joinable()) {
        accept_thread.join();
      }

      attached.store(false);
      cleanup_winsock();
    }

    void cleanup_winsock() {
      if (wsa_started) {
        ::WSACleanup();
        wsa_started = false;
      }
    }

    void update_input(const gamepad_state_t &state) {
      std::lock_guard lock(input_mutex);
      input_state = state;
    }

    gamepad_state_t current_input() {
      std::lock_guard lock(input_mutex);
      return input_state;
    }

    void accept_loop() {
      while (!stop_requested.load()) {
        const auto client = ::accept(listen_socket.load(), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
          break;
        }

        int nodelay = 1;
        ::setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&nodelay), sizeof(nodelay));

        active_client.store(client);
        serve_client(client);
        const auto tracked_client = active_client.exchange(INVALID_SERVER_SOCKET);
        if (tracked_client != INVALID_SERVER_SOCKET) {
          ::closesocket(tracked_client);
        }
        attached.store(false);
      }
    }

    void serve_client(SOCKET client) {
      std::array<std::uint8_t, usbip::OP_COMMON_SIZE> op_header {};
      if (!recv_exact(client, op_header)) {
        return;
      }

      const auto common = usbip::parse_op_common(op_header);
      if (common.code == usbip::op_code::req_devlist) {
        const auto reply = dualsense_usbip::handle_op_request(op_header);
        if (!reply.empty()) {
          send_all(client, reply);
        }
        return;
      }

      if (common.code != usbip::op_code::req_import) {
        return;
      }

      std::array<std::uint8_t, usbip::OP_COMMON_SIZE + usbip::USB_DEVICE_BUSID_SIZE> import_request {};
      std::ranges::copy(op_header, import_request.begin());
      if (!recv_exact(client, std::span {import_request}.subspan(usbip::OP_COMMON_SIZE))) {
        return;
      }

      const auto import_reply = dualsense_usbip::handle_op_request(import_request);
      if (import_reply.empty() || !send_all(client, import_reply)) {
        return;
      }

      if (usbip::read_be32(import_reply, 4) != 0) {
        return;
      }

      BOOST_LOG(info) << "DualSense Edge USB/IP: client imported virtual controller"sv;
      attached.store(true);
      serve_imported_session(client);
      BOOST_LOG(info) << "DualSense Edge USB/IP: client session ended"sv;
    }

    void serve_imported_session(SOCKET client) {
      session_t session;
      session.update_input(current_input());

      std::atomic_bool session_stop {false};
      std::mutex session_mutex;
      std::mutex send_mutex;

      std::thread input_sender {[&] {
        while (!session_stop.load()) {
          std::vector<std::uint8_t> reply;
          {
            std::lock_guard lock(session_mutex);
            session.update_input(current_input());
            reply = session.take_next_interrupt_in_reply();
          }

          if (!reply.empty()) {
            std::lock_guard lock(send_mutex);
            if (!send_all(client, reply)) {
              session_stop.store(true);
              break;
            }
          }

          std::this_thread::sleep_for(INPUT_REPLY_INTERVAL);
        }
      }};

      std::array<std::uint8_t, usbip::SUBMIT_HEADER_SIZE> header {};
      while (!session_stop.load() && recv_exact(client, header)) {
        const auto basic = usbip::parse_basic_header(header);

        if (basic.command_code == usbip::command::cmd_submit) {
          const auto request = usbip::parse_submit_request(header);
          if (!dualsense_usbip::is_valid_out_payload_length(request)) {
            BOOST_LOG(warning)
              << "DualSense Edge USB/IP: rejecting oversized OUT payload length "sv
              << request.transfer_buffer_length
              << " on endpoint "sv
              << request.header.endpoint;
            break;
          }

          std::vector<std::uint8_t> out_payload;
          if (request.header.transfer_direction == usbip::direction::out &&
              request.transfer_buffer_length > 0) {
            out_payload.resize(request.transfer_buffer_length);
            if (!recv_exact(client, out_payload)) {
              break;
            }
          }

          submit_result_t result;
          {
            std::lock_guard lock(session_mutex);
            result = session.handle_submit(request, out_payload);
          }

          if (result.has_output_report && output_cb) {
            output_cb(result.output_report);
          }

          if (!result.reply.empty()) {
            std::lock_guard lock(send_mutex);
            if (!send_all(client, result.reply)) {
              break;
            }
          }
        } else if (basic.command_code == usbip::command::cmd_unlink) {
          const auto request = usbip::parse_unlink_request(header);
          unlink_result_t result;
          {
            std::lock_guard lock(session_mutex);
            result = session.handle_unlink(request);
          }

          std::lock_guard lock(send_mutex);
          if (!send_all(client, result.reply)) {
            break;
          }
        } else {
          break;
        }
      }

      session_stop.store(true);
      if (input_sender.joinable()) {
        input_sender.join();
      }
    }

    std::atomic_bool running {false};
    std::atomic_bool attached {false};
    std::atomic_bool stop_requested {false};
    std::atomic<SOCKET> listen_socket {INVALID_SERVER_SOCKET};
    std::atomic<SOCKET> active_client {INVALID_SERVER_SOCKET};
    std::thread accept_thread;

    bool wsa_started {};
    server_t::output_cb_t output_cb;

    std::mutex input_mutex;
    gamepad_state_t input_state {};
  };

  server_t::server_t():
      impl_(std::make_unique<impl_t>()) {}

  server_t::~server_t() = default;

  bool server_t::start(output_cb_t output_cb, std::uint16_t port) {
    return impl_->start(std::move(output_cb), port);
  }

  void server_t::stop() {
    impl_->stop();
  }

  bool server_t::is_running() const {
    return impl_->running.load();
  }

  bool server_t::is_attached() const {
    return impl_->attached.load();
  }

  void server_t::update_input(const gamepad_state_t &state) {
    impl_->update_input(state);
  }
}  // namespace platf::dualsense_usbip
