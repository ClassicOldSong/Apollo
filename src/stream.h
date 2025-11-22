/**
 * @file src/stream.h
 * @brief Declarations for the streaming protocols.
 */
#pragma once

// standard includes
#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// lib includes
#include <boost/asio.hpp>

extern "C" {
  // clang-format off
#include <moonlight-common-c/src/Limelight-internal.h>
  // clang-format on
}

// local includes
#include "audio.h"
#include "auto_bitrate.h"
#include "crypto.h"
#include "input.h"
#include "network.h"
#include "platform/common.h"
#include "sync.h"
#include "thread_safe.h"
#include "utility.h"
#include "video.h"

namespace stream {
  constexpr auto VIDEO_STREAM_PORT = 9;
  constexpr auto CONTROL_PORT = 10;
  constexpr auto AUDIO_STREAM_PORT = 11;

  // Forward declarations
  struct session_t;

  namespace session {
    enum class state_e : int;
  }  // namespace session

  struct config_t {
    audio::config_t audio;
    video::config_t monitor;

    int packetsize;
    int minRequiredFecPackets;
    int mlFeatureFlags;
    int controlProtocolType;
    int audioQosType;
    int videoQosType;

    uint32_t encryptionFlagsEnabled;

    std::optional<int> gcmap;
    bool autoBitrateEnabled;
  };

  enum class socket_e : int {
    video,  ///< Video
    audio  ///< Audio
  };

  using av_session_id_t = std::variant<boost::asio::ip::address, std::string>;  // IP address or SS-Ping-Payload from RTSP handshake
  using message_queue_t = std::shared_ptr<safe::queue_t<std::pair<boost::asio::ip::udp::endpoint, std::string>>>;
  using message_queue_queue_t = std::shared_ptr<safe::queue_t<std::tuple<socket_e, av_session_id_t, message_queue_t>>>;

  class control_server_t {
  public:
    int bind(net::af_e address_family, std::uint16_t port) {
      _host = net::host_create(address_family, _addr, port);
      return !(bool) _host;
    }

    // Get session associated with address.
    // If none are found, try to find a session not yet claimed. (It will be marked by a port of value 0
    // If none of those are found, return nullptr
    session_t *get_session(const net::peer_t peer, uint32_t connect_data);

    // Circular dependency:
    //   iterate refers to session
    //   session refers to broadcast_ctx_t
    //   broadcast_ctx_t refers to control_server_t
    // Therefore, iterate is implemented further down the source file
    void iterate(std::chrono::milliseconds timeout);

    /**
     * @brief Call the handler for a given control stream message.
     * @param type The message type.
     * @param session The session the message was received on.
     * @param payload The payload of the message.
     * @param reinjected `true` if this message is being reprocessed after decryption.
     */
    void call(std::uint16_t type, session_t *session, const std::string_view &payload, bool reinjected);

    void map(uint16_t type, std::function<void(session_t *, const std::string_view &)> cb) {
      _map_type_cb.emplace(type, std::move(cb));
    }

    int send(const std::string_view &payload, net::peer_t peer) {
      auto packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
      if (enet_peer_send(peer, 0, packet)) {
        enet_packet_destroy(packet);
        return -1;
      }
      return 0;
    }

    void flush() {
      enet_host_flush(_host.get());
    }

    // Callbacks
    std::unordered_map<std::uint16_t, std::function<void(session_t *, const std::string_view &)>> _map_type_cb;

    // All active sessions (including those still waiting for a peer to connect)
    sync_util::sync_t<std::vector<session_t *>> _sessions;

    // ENet peer to session mapping for sessions with a peer connected
    sync_util::sync_t<std::map<net::peer_t, session_t *>> _peer_to_session;

    ENetAddress _addr;
    net::host_t _host;
  };

  struct broadcast_ctx_t {
    message_queue_queue_t message_queue_queue;

    std::thread recv_thread;
    std::thread video_thread;
    std::thread audio_thread;
    std::thread control_thread;

    boost::asio::io_context io_context;

    boost::asio::ip::udp::socket video_sock {io_context};
    boost::asio::ip::udp::socket audio_sock {io_context};

    control_server_t control_server;
  };

#pragma pack(push, 1)
  struct audio_fec_packet_t {
    RTP_PACKET rtp;
    AUDIO_FEC_HEADER fecHeader;
  };
#pragma pack(pop)

  struct session_t {
    config_t config;

    safe::mail_t mail;

    std::shared_ptr<input::input_t> input;

    std::thread audioThread;
    std::thread videoThread;

    std::chrono::steady_clock::time_point pingTimeout;

    safe::shared_t<broadcast_ctx_t>::ptr_t broadcast_ref;

    boost::asio::ip::address localAddress;

    std::unique_ptr<auto_bitrate::AutoBitrateController> auto_bitrate_controller;

    struct {
      std::string ping_payload;

      int lowseq;
      boost::asio::ip::udp::endpoint peer;

      std::optional<crypto::cipher::gcm_t> cipher;
      std::uint64_t gcm_iv_counter;

      safe::mail_raw_t::event_t<bool> idr_events;
      safe::mail_raw_t::event_t<std::pair<int64_t, int64_t>> invalidate_ref_frames_events;
      safe::mail_raw_t::event_t<int> bitrate_update_event;

      std::unique_ptr<platf::deinit_t> qos;
    } video;

    struct {
      crypto::cipher::cbc_t cipher;
      std::string ping_payload;

      std::uint16_t sequenceNumber;
      // avRiKeyId == util::endian::big(First (sizeof(avRiKeyId)) bytes of launch_session->iv)
      std::uint32_t avRiKeyId;
      std::uint32_t timestamp;
      boost::asio::ip::udp::endpoint peer;

      util::buffer_t<char> shards;
      util::buffer_t<uint8_t *> shards_p;

      audio_fec_packet_t fec_packet;
      std::unique_ptr<platf::deinit_t> qos;
    } audio;

    struct {
      crypto::cipher::gcm_t cipher;
      crypto::aes_t legacy_input_enc_iv;  // Only used when the client doesn't support full control stream encryption
      crypto::aes_t incoming_iv;
      crypto::aes_t outgoing_iv;

      std::uint32_t connect_data;  // Used for new clients with ML_FF_SESSION_ID_V1
      std::string expected_peer_address;  // Only used for legacy clients without ML_FF_SESSION_ID_V1

      net::peer_t peer;
      std::uint32_t seq;

      platf::feedback_queue_t feedback_queue;
      safe::mail_raw_t::event_t<video::hdr_info_t> hdr_queue;
    } control;

    std::uint32_t launch_session_id;
    std::string device_name;
    std::string device_uuid;
    crypto::PERM permission;

    std::list<crypto::command_entry_t> do_cmds;
    std::list<crypto::command_entry_t> undo_cmds;

    safe::mail_raw_t::event_t<bool> shutdown_event;
    safe::signal_t controlEnd;

    std::atomic<session::state_e> state;
  };

  namespace session {
    enum class state_e : int {
      STOPPED,  ///< The session is stopped
      STOPPING,  ///< The session is stopping
      STARTING,  ///< The session is starting
      RUNNING,  ///< The session is running
    };

    std::shared_ptr<session_t> alloc(config_t &config, rtsp_stream::launch_session_t &launch_session);
    std::string uuid(const session_t& session);
    bool uuid_match(const session_t& session, const std::string_view& uuid);
    bool update_device_info(session_t& session, const std::string& name, const crypto::PERM& newPerm);
    int start(session_t &session, const std::string &addr_string);
    void stop(session_t &session);
    void graceful_stop(session_t& session);
    void join(session_t &session);
    state_e state(session_t &session);
    inline bool send(session_t& session, const std::string_view &payload);
  }  // namespace session
}  // namespace stream
