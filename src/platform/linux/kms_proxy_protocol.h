/**
 * @file src/platform/linux/kms_proxy_protocol.h
 * @brief Wire protocol shared by the kms-server-proxy helper and its client.
 *
 * When Sunshine lacks CAP_SYS_ADMIN, drmModeGetFB2() succeeds but returns no
 * GEM handles, so DRM framebuffers cannot be exported for KMS capture. As a
 * fallback, a small root helper (kms-server-proxy, launched via pkexec) runs
 * the privileged DRM calls and passes the resulting DMA-BUF file descriptors
 * back over a Unix socket with SCM_RIGHTS, in the same way that
 * gpu-screen-recorder's gsr-kms-server does.
 *
 * This header is included by both the helper (which must not depend on any
 * Sunshine code) and the in-process client, so it may only use the standard
 * library.
 */
#pragma once

#include <cstdint>

namespace kmsp {

  constexpr std::uint32_t MAGIC = 0x4B4D5350;  // "KMSP"
  constexpr std::uint32_t VERSION = 1;

  enum class request_e : std::uint32_t {
    get_fb = 1,  ///< Export the plane's current framebuffer as DMA-BUF fds.
    get_cursor = 2,  ///< Read cursor framebuffer pixels (server-side mmap).
  };

  /**
   * @brief Sent by the server immediately after connecting.
   */
  struct hello_t {
    std::uint32_t magic;
    std::uint32_t version;
  };

  /**
   * @brief Client request. Fixed size for both request types.
   */
  struct request_t {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t type;  ///< One of request_e.
    std::uint32_t plane_id;  ///< DRM plane whose framebuffer is wanted.

    // get_cursor only: source rectangle to copy, in pixels
    std::uint32_t src_x;
    std::uint32_t src_y;
    std::uint32_t src_w;
    std::uint32_t src_h;
  };

  /**
   * @brief Reply to get_fb. On success, fd_count DMA-BUF file descriptors
   *        follow via SCM_RIGHTS ancillary data (one per framebuffer plane,
   *        in plane order).
   */
  struct fb_reply_t {
    std::int32_t result;  ///< 0 on success, negative errno on failure.
    std::uint32_t fb_id;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pixel_format;
    std::uint64_t modifier;
    std::uint32_t pitches[4];
    std::uint32_t offsets[4];
    std::uint32_t fd_count;
  };

  /**
   * @brief Reply to get_cursor. On success, data_size bytes of tightly packed
   *        32-bit pixel data (src_w * src_h * 4) follow on the stream.
   */
  struct cursor_reply_t {
    std::int32_t result;  ///< 0 on success, negative errno on failure.
    std::uint32_t width;  ///< Full framebuffer width.
    std::uint32_t height;  ///< Full framebuffer height.
    std::uint32_t pixel_format;
    std::uint64_t modifier;
    std::uint32_t data_size;
  };

  // Sanity limit for cursor pixel payloads (covers even absurd cursor planes)
  constexpr std::uint32_t MAX_CURSOR_BYTES = 16 * 1024 * 1024;

}  // namespace kmsp
