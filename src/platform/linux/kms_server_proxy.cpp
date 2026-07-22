/**
 * @file src/platform/linux/kms_server_proxy.cpp
 * @brief Privileged KMS helper, in the style of gpu-screen-recorder's gsr-kms-server.
 *
 * Sunshine launches this helper via `pkexec` when it lacks CAP_SYS_ADMIN.
 * drmModeGetFB2() only returns GEM handles to DRM master or CAP_SYS_ADMIN
 * callers (checked per-ioctl on the calling process), so an unprivileged
 * Sunshine cannot export framebuffers itself. This helper runs as root,
 * performs the privileged DRM calls, and passes DMA-BUF file descriptors
 * (and cursor pixel data) back over a Unix socket.
 *
 * Usage: kms-server-proxy <socket-path> <card-path>
 *
 * The unprivileged client creates <socket-path> inside a mode-0700 directory
 * it owns and listens on it; this helper connects to it. The helper exits as
 * soon as the socket reaches EOF, so it never outlives the client.
 *
 * A polkit rule keyed on this binary's basename allows passwordless launch,
 * e.g. /etc/polkit-1/rules.d/49-gsr-kms.rules matching "/kms-server-proxy".
 *
 * This file must build standalone: libc + libdrm only, no Sunshine headers
 * except the wire protocol.
 */
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <linux/dma-buf.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms_proxy_protocol.h"

namespace {

  int card_fd = -1;

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
        return false;  // EOF
      }
      p += rd;
      size -= (size_t) rd;
    }
    return true;
  }

  /**
   * @brief Send a reply struct plus DMA-BUF fds as SCM_RIGHTS ancillary data.
   */
  bool send_with_fds(int sock, const void *data, size_t size, const int *fds, size_t fd_count) {
    struct msghdr msg = {};
    struct iovec iov;
    iov.iov_base = (void *) data;
    iov.iov_len = size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char cmsg_buf[CMSG_SPACE(sizeof(int) * 4)] = {};
    if (fd_count > 0) {
      msg.msg_control = cmsg_buf;
      msg.msg_controllen = CMSG_SPACE(sizeof(int) * fd_count);

      struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fd_count);
      memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * fd_count);
    }

    while (true) {
      ssize_t written = sendmsg(sock, &msg, MSG_NOSIGNAL);
      if (written < 0 && errno == EINTR) {
        continue;
      }
      return written == (ssize_t) size;
    }
  }

  void close_gem_handles(const uint32_t *handles) {
    // The same GEM handle may back multiple planes; close each one only once
    std::set<uint32_t> closed;
    for (int i = 0; i < 4; i++) {
      if (handles[i] && closed.insert(handles[i]).second) {
        struct drm_gem_close close_args = {};
        close_args.handle = handles[i];
        drmIoctl(card_fd, DRM_IOCTL_GEM_CLOSE, &close_args);
      }
    }
  }

  void handle_get_fb(int sock, const kmsp::request_t &req) {
    kmsp::fb_reply_t reply = {};
    int fds[4] = {-1, -1, -1, -1};
    size_t fd_count = 0;
    drmModeFB2 *fb2 = nullptr;

    drmModePlane *plane = drmModeGetPlane(card_fd, req.plane_id);
    if (!plane || !plane->fb_id) {
      reply.result = -ENOENT;
      goto respond;
    }

    fb2 = drmModeGetFB2(card_fd, plane->fb_id);
    if (!fb2) {
      reply.result = -errno;
      goto respond;
    }

    if (!fb2->handles[0]) {
      reply.result = -EACCES;
      goto respond;
    }

    reply.fb_id = fb2->fb_id;
    reply.width = fb2->width;
    reply.height = fb2->height;
    reply.pixel_format = fb2->pixel_format;
    reply.modifier = (fb2->flags & DRM_MODE_FB_MODIFIERS) ? fb2->modifier : DRM_FORMAT_MOD_INVALID;
    memcpy(reply.pitches, fb2->pitches, sizeof(reply.pitches));
    memcpy(reply.offsets, fb2->offsets, sizeof(reply.offsets));

    for (int i = 0; i < 4; i++) {
      if (!fb2->handles[i]) {
        break;
      }
      if (drmPrimeHandleToFD(card_fd, fb2->handles[i], O_CLOEXEC, &fds[i])) {
        reply.result = -errno;
        goto respond;
      }
      fd_count++;
    }
    reply.fd_count = fd_count;
    reply.result = 0;

  respond:
    if (!send_with_fds(sock, &reply, sizeof(reply), fds, reply.result == 0 ? fd_count : 0)) {
      fprintf(stderr, "kms-server-proxy: failed to send fb reply: %s\n", strerror(errno));
    }

    for (int i = 0; i < 4; i++) {
      if (fds[i] >= 0) {
        close(fds[i]);
      }
    }
    if (fb2) {
      close_gem_handles(fb2->handles);
      drmModeFreeFB2(fb2);
    }
    if (plane) {
      drmModeFreePlane(plane);
    }
  }

  void handle_get_cursor(int sock, const kmsp::request_t &req) {
    kmsp::cursor_reply_t reply = {};
    std::vector<uint8_t> pixels;
    drmModeFB2 *fb2 = nullptr;
    int prime_fd = -1;
    void *mapped = MAP_FAILED;
    size_t mapped_size = 0;

    drmModePlane *plane = drmModeGetPlane(card_fd, req.plane_id);
    if (!plane || !plane->fb_id) {
      reply.result = -ENOENT;
      goto respond;
    }

    fb2 = drmModeGetFB2(card_fd, plane->fb_id);
    if (!fb2 || !fb2->handles[0]) {
      reply.result = fb2 ? -EACCES : -errno;
      goto respond;
    }

    reply.width = fb2->width;
    reply.height = fb2->height;
    reply.pixel_format = fb2->pixel_format;
    reply.modifier = (fb2->flags & DRM_MODE_FB_MODIFIERS) ? fb2->modifier : DRM_FORMAT_MOD_INVALID;

    // Validate the requested source rectangle against the framebuffer
    if (req.src_w == 0 || req.src_h == 0 ||
        req.src_x + req.src_w > fb2->width ||
        req.src_y + req.src_h > fb2->height ||
        (uint64_t) req.src_w * req.src_h * 4 > kmsp::MAX_CURSOR_BYTES) {
      reply.result = -EINVAL;
      goto respond;
    }

    mapped_size = (size_t) fb2->pitches[0] * fb2->height;

    // Try mapping the DMA-BUF directly first
    if (drmPrimeHandleToFD(card_fd, fb2->handles[0], O_CLOEXEC, &prime_fd) == 0) {
      mapped = mmap(nullptr, mapped_size, PROT_READ, MAP_SHARED, prime_fd, fb2->offsets[0]);
    }

    // NVIDIA requires mapping cursor FBs as dumb buffers instead
    if (mapped == MAP_FAILED) {
      drm_mode_map_dumb map = {};
      map.handle = fb2->handles[0];
      if (drmIoctl(card_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) == 0) {
        mapped = mmap(nullptr, mapped_size, PROT_READ, MAP_SHARED, card_fd, map.offset);
      }
    }

    if (mapped == MAP_FAILED) {
      reply.result = -errno;
      goto respond;
    }

    {
      if (prime_fd >= 0) {
        struct dma_buf_sync sync = {};
        sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
        drmIoctl(prime_fd, DMA_BUF_IOCTL_SYNC, &sync);
      }

      pixels.resize((size_t) req.src_w * req.src_h * 4);
      for (uint32_t y = 0; y < req.src_h; y++) {
        memcpy(&pixels[(size_t) y * req.src_w * 4],
               &((uint8_t *) mapped)[(size_t) (y + req.src_y) * fb2->pitches[0] + (size_t) req.src_x * 4],
               (size_t) req.src_w * 4);
      }

      if (prime_fd >= 0) {
        struct dma_buf_sync sync = {};
        sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        drmIoctl(prime_fd, DMA_BUF_IOCTL_SYNC, &sync);
      }
    }

    reply.data_size = pixels.size();
    reply.result = 0;

  respond:
    if (!send_all(sock, &reply, sizeof(reply))) {
      fprintf(stderr, "kms-server-proxy: failed to send cursor reply: %s\n", strerror(errno));
    } else if (reply.result == 0 && !pixels.empty()) {
      if (!send_all(sock, pixels.data(), pixels.size())) {
        fprintf(stderr, "kms-server-proxy: failed to send cursor pixels: %s\n", strerror(errno));
      }
    }

    if (mapped != MAP_FAILED) {
      munmap(mapped, mapped_size);
    }
    if (prime_fd >= 0) {
      close(prime_fd);
    }
    if (fb2) {
      close_gem_handles(fb2->handles);
      drmModeFreeFB2(fb2);
    }
    if (plane) {
      drmModeFreePlane(plane);
    }
  }

}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <socket-path> <card-path>\n", argv[0]);
    return 1;
  }

  if (geteuid() != 0) {
    fprintf(stderr, "kms-server-proxy: must run as root (via pkexec)\n");
    return 1;
  }

  const char *socket_path = argv[1];
  const char *card_path = argv[2];

  // Only DRM card nodes may be opened with root privileges
  char resolved[PATH_MAX];
  if (!realpath(card_path, resolved) || strncmp(resolved, "/dev/dri/card", 13) != 0) {
    fprintf(stderr, "kms-server-proxy: invalid card path: %s\n", card_path);
    return 1;
  }

  card_fd = open(resolved, O_RDWR | O_CLOEXEC);
  if (card_fd < 0) {
    fprintf(stderr, "kms-server-proxy: couldn't open %s: %s\n", resolved, strerror(errno));
    return 1;
  }

  if (drmSetClientCap(card_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
    fprintf(stderr, "kms-server-proxy: no universal planes support\n");
    return 1;
  }

  int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sock < 0) {
    fprintf(stderr, "kms-server-proxy: socket: %s\n", strerror(errno));
    return 1;
  }

  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(addr.sun_path)) {
    fprintf(stderr, "kms-server-proxy: socket path too long\n");
    return 1;
  }
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (connect(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    fprintf(stderr, "kms-server-proxy: connect %s: %s\n", socket_path, strerror(errno));
    return 1;
  }

  // Only serve the user that invoked pkexec (or root)
  const char *pkexec_uid = getenv("PKEXEC_UID");
  if (pkexec_uid) {
    struct ucred cred = {};
    socklen_t cred_len = sizeof(cred);
    if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) == 0) {
      uid_t expected = (uid_t) strtoul(pkexec_uid, nullptr, 10);
      if (cred.uid != expected && cred.uid != 0) {
        fprintf(stderr, "kms-server-proxy: peer uid %u does not match invoking uid %u\n", cred.uid, expected);
        return 1;
      }
    }
  }

  kmsp::hello_t hello = {kmsp::MAGIC, kmsp::VERSION};
  if (!send_all(sock, &hello, sizeof(hello))) {
    return 1;
  }

  // Serve requests until the client closes the socket (or exits)
  while (true) {
    kmsp::request_t req = {};
    if (!recv_all(sock, &req, sizeof(req))) {
      break;
    }

    if (req.magic != kmsp::MAGIC || req.version != kmsp::VERSION) {
      fprintf(stderr, "kms-server-proxy: bad request magic/version\n");
      break;
    }

    switch ((kmsp::request_e) req.type) {
      case kmsp::request_e::get_fb:
        handle_get_fb(sock, req);
        break;
      case kmsp::request_e::get_cursor:
        handle_get_cursor(sock, req);
        break;
      default:
        fprintf(stderr, "kms-server-proxy: unknown request type %u\n", req.type);
        return 1;
    }
  }

  close(sock);
  close(card_fd);
  return 0;
}
