/**
 * @file src/platform/linux/kms_proxy.cpp
 * @brief Client implementation for the kms-server-proxy privileged KMS helper.
 */
#include "kms_proxy.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "src/logging.h"

using namespace std::literals;

namespace kmsp {

  namespace {

    constexpr int SPAWN_TIMEOUT_MS = 15000;  // pkexec may need to consult polkit
    constexpr int REPLY_TIMEOUT_S = 5;

    bool send_all(int sock, const void *data, size_t size) {
      auto p = (const uint8_t *) data;
      while (size > 0) {
        ssize_t written = send(sock, p, size, MSG_NOSIGNAL);
        if (written < 0) {
          if (errno == EINTR) {
            continue;
          }
          return false;
        }
        p += written;
        size -= (size_t) written;
      }
      return true;
    }

    bool recv_all(int sock, void *data, size_t size) {
      auto p = (uint8_t *) data;
      while (size > 0) {
        ssize_t rd = recv(sock, p, size, 0);
        if (rd < 0) {
          if (errno == EINTR) {
            continue;
          }
          return false;
        }
        if (rd == 0) {
          return false;
        }
        p += rd;
        size -= (size_t) rd;
      }
      return true;
    }

    /**
     * @brief Receive a reply struct plus up to max_fds SCM_RIGHTS fds.
     */
    bool recv_with_fds(int sock, void *data, size_t size, file_t *fds_out, size_t max_fds) {
      struct msghdr msg = {};
      struct iovec iov;
      iov.iov_base = data;
      iov.iov_len = size;
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;

      char cmsg_buf[CMSG_SPACE(sizeof(int) * 4)] = {};
      msg.msg_control = cmsg_buf;
      msg.msg_controllen = sizeof(cmsg_buf);

      ssize_t rd;
      do {
        rd = recvmsg(sock, &msg, MSG_CMSG_CLOEXEC);
      } while (rd < 0 && errno == EINTR);

      if (rd <= 0) {
        return false;
      }

      // Adopt any received fds immediately so they can't leak
      for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
          continue;
        }

        size_t count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        int received[4];
        memcpy(received, CMSG_DATA(cmsg), std::min(count, (size_t) 4) * sizeof(int));
        for (size_t i = 0; i < count; i++) {
          if (i < max_fds && i < 4) {
            fds_out[i].el = received[i];
          } else if (i < 4) {
            close(received[i]);
          }
        }
      }

      // The reply struct is small enough to arrive in one datagram-like read,
      // but finish any partial read to be safe
      if ((size_t) rd < size) {
        return recv_all(sock, (uint8_t *) data + rd, size - rd);
      }
      return true;
    }

    /**
     * @brief Locate the helper binary next to the running executable.
     */
    std::string find_helper() {
      char exe[PATH_MAX];
      ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
      if (len <= 0) {
        return {};
      }
      exe[len] = '\0';

      auto slash = strrchr(exe, '/');
      if (!slash) {
        return {};
      }

      std::string path(exe, slash - exe);
      path += "/kms-server-proxy";
      if (access(path.c_str(), X_OK) != 0) {
        return {};
      }
      return path;
    }

  }  // namespace

  std::shared_ptr<client_t> client_t::get(const std::string &card_path) {
    static std::mutex cache_mtx;
    static std::map<std::string, std::shared_ptr<client_t>> cache;
    static std::map<std::string, bool> attempted;

    std::lock_guard lock {cache_mtx};

    auto it = cache.find(card_path);
    if (it != cache.end() && !it->second->dead) {
      return it->second;
    }

    // Don't retry pkexec over and over if the first attempt failed
    if (attempted[card_path] && it == cache.end()) {
      return nullptr;
    }
    attempted[card_path] = true;

    std::shared_ptr<client_t> client {new client_t()};
    if (!client->spawn(card_path)) {
      cache.erase(card_path);
      return nullptr;
    }

    cache[card_path] = client;
    return client;
  }

  bool client_t::spawn(const std::string &card_path) {
    auto helper = find_helper();
    if (helper.empty()) {
      BOOST_LOG(error) << "kms-server-proxy helper not found next to the sunshine binary"sv;
      return false;
    }

    char dir_template[] = "/tmp/apollo-kms-XXXXXX";
    if (!mkdtemp(dir_template)) {
      BOOST_LOG(error) << "Couldn't create temporary directory for KMS proxy socket: "sv << strerror(errno);
      return false;
    }
    std::string sock_dir = dir_template;
    std::string sock_path = sock_dir + "/sock";

    auto cleanup_dir = [&]() {
      unlink(sock_path.c_str());
      rmdir(sock_dir.c_str());
    };

    file_t listener {socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    if (listener.el < 0) {
      cleanup_dir();
      return false;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listener.el, (struct sockaddr *) &addr, sizeof(addr)) || listen(listener.el, 1)) {
      BOOST_LOG(error) << "Couldn't listen on KMS proxy socket: "sv << strerror(errno);
      cleanup_dir();
      return false;
    }

    BOOST_LOG(info) << "Starting KMS capture helper: pkexec "sv << helper << " (requires a polkit rule for passwordless use)"sv;

    child = fork();
    if (child < 0) {
      cleanup_dir();
      return false;
    }
    if (child == 0) {
      execlp("pkexec", "pkexec", helper.c_str(), sock_path.c_str(), card_path.c_str(), nullptr);
      _exit(127);
    }

    // Wait for the helper to connect, watching for early pkexec failure
    int waited_ms = 0;
    bool connected = false;
    while (waited_ms < SPAWN_TIMEOUT_MS) {
      struct pollfd pfd = {listener.el, POLLIN, 0};
      int rc = poll(&pfd, 1, 100);
      if (rc > 0) {
        connected = true;
        break;
      }

      int status;
      if (waitpid(child, &status, WNOHANG) == child) {
        BOOST_LOG(error) << "KMS capture helper exited early (pkexec status "sv
                         << (WIFEXITED(status) ? WEXITSTATUS(status) : -1)
                         << "). Install a polkit rule allowing kms-server-proxy, or run [sudo setcap cap_sys_admin+p $(readlink -f $(which sunshine))]"sv;
        child = -1;
        cleanup_dir();
        return false;
      }

      waited_ms += 100;
    }

    if (!connected) {
      BOOST_LOG(error) << "Timed out waiting for the KMS capture helper to connect"sv;
      cleanup_dir();
      return false;
    }

    sock.el = accept4(listener.el, nullptr, nullptr, SOCK_CLOEXEC);
    cleanup_dir();  // connected socket no longer needs the filesystem entry
    if (sock.el < 0) {
      return false;
    }

    // Only trust a root helper on the other end
    struct ucred cred = {};
    socklen_t cred_len = sizeof(cred);
    if (getsockopt(sock.el, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) || cred.uid != 0) {
      BOOST_LOG(error) << "KMS proxy peer is not root, rejecting"sv;
      return false;
    }

    struct timeval tv = {REPLY_TIMEOUT_S, 0};
    setsockopt(sock.el, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    hello_t hello = {};
    if (!recv_all(sock.el, &hello, sizeof(hello)) || hello.magic != MAGIC || hello.version != VERSION) {
      BOOST_LOG(error) << "KMS proxy handshake failed (helper version mismatch?)"sv;
      return false;
    }

    BOOST_LOG(info) << "KMS capture helper connected for "sv << card_path;
    return true;
  }

  client_t::~client_t() {
    if (sock.el >= 0) {
      close(sock.el);
      sock.el = -1;
    }

    if (child > 0) {
      // The helper exits on socket EOF; give it a moment, then move on.
      // (We can't signal it: pkexec runs it as root.)
      for (int i = 0; i < 20; i++) {
        if (waitpid(child, nullptr, WNOHANG) != 0) {
          return;
        }
        usleep(50000);
      }
    }
  }

  void client_t::mark_dead() {
    dead = true;
    if (sock.el >= 0) {
      close(sock.el);
      sock.el = -1;
    }
  }

  bool client_t::transact(const request_t &req, void *reply, size_t reply_size, file_t *fds_out, size_t max_fds) {
    std::lock_guard lock {mtx};

    if (dead || sock.el < 0) {
      return false;
    }

    if (!send_all(sock.el, &req, sizeof(req))) {
      BOOST_LOG(error) << "KMS proxy request failed: "sv << strerror(errno);
      mark_dead();
      return false;
    }

    bool ok;
    if (fds_out) {
      ok = recv_with_fds(sock.el, reply, reply_size, fds_out, max_fds);
    } else {
      ok = recv_all(sock.el, reply, reply_size);
    }

    if (!ok) {
      BOOST_LOG(error) << "KMS proxy reply failed: "sv << strerror(errno);
      mark_dead();
    }
    return ok;
  }

  bool client_t::get_fb(std::uint32_t plane_id, fb_reply_t &reply, file_t fds[4]) {
    request_t req = {};
    req.magic = MAGIC;
    req.version = VERSION;
    req.type = (std::uint32_t) request_e::get_fb;
    req.plane_id = plane_id;

    if (!transact(req, &reply, sizeof(reply), fds, 4)) {
      return false;
    }

    if (reply.result != 0) {
      BOOST_LOG(debug) << "KMS proxy get_fb failed for plane "sv << plane_id << ": "sv << strerror(-reply.result);
      return false;
    }

    return true;
  }

  bool client_t::get_cursor(std::uint32_t plane_id, std::uint32_t src_x, std::uint32_t src_y, std::uint32_t src_w, std::uint32_t src_h, cursor_reply_t &reply, std::vector<std::uint8_t> &pixels) {
    request_t req = {};
    req.magic = MAGIC;
    req.version = VERSION;
    req.type = (std::uint32_t) request_e::get_cursor;
    req.plane_id = plane_id;
    req.src_x = src_x;
    req.src_y = src_y;
    req.src_w = src_w;
    req.src_h = src_h;

    if (!transact(req, &reply, sizeof(reply), nullptr, 0)) {
      return false;
    }

    if (reply.result != 0) {
      BOOST_LOG(debug) << "KMS proxy get_cursor failed for plane "sv << plane_id << ": "sv << strerror(-reply.result);
      return false;
    }

    if (reply.data_size == 0 || reply.data_size > MAX_CURSOR_BYTES || reply.data_size != src_w * src_h * 4) {
      BOOST_LOG(error) << "KMS proxy returned unexpected cursor payload size "sv << reply.data_size;
      mark_dead();
      return false;
    }

    pixels.resize(reply.data_size);

    std::lock_guard lock {mtx};
    if (!recv_all(sock.el, pixels.data(), pixels.size())) {
      BOOST_LOG(error) << "KMS proxy cursor payload receive failed"sv;
      mark_dead();
      return false;
    }

    return true;
  }

}  // namespace kmsp
