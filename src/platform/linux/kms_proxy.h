/**
 * @file src/platform/linux/kms_proxy.h
 * @brief Client for the kms-server-proxy privileged KMS helper.
 *
 * When Sunshine runs without CAP_SYS_ADMIN, this client spawns the
 * kms-server-proxy helper via pkexec and requests DRM framebuffer exports
 * (DMA-BUF fds) and cursor pixel reads from it. See kms_proxy_protocol.h
 * for the rationale and wire format.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

#include "kms_proxy_protocol.h"
#include "misc.h"

namespace kmsp {

  class client_t {
  public:
    /**
     * @brief Get (or spawn) the helper for a DRM card. Cached per card path,
     *        so pkexec runs at most once per card per process lifetime.
     *
     * @param card_path Path to the DRM card node, e.g. /dev/dri/card1.
     * @return Connected client, or nullptr when the helper isn't available.
     */
    static std::shared_ptr<client_t> get(const std::string &card_path);

    ~client_t();

    /**
     * @brief Export the plane's current framebuffer as DMA-BUF fds.
     *
     * @param plane_id DRM plane ID.
     * @param reply Output framebuffer metadata.
     * @param fds Output array of 4 fds; entries beyond reply.fd_count stay -1.
     * @return True on success.
     */
    bool get_fb(std::uint32_t plane_id, fb_reply_t &reply, file_t fds[4]);

    /**
     * @brief Read a rectangle of cursor framebuffer pixels (server-side mmap,
     *        including the dumb-buffer fallback required on NVIDIA).
     *
     * @param plane_id DRM cursor plane ID.
     * @param src_x Source rectangle X offset in pixels.
     * @param src_y Source rectangle Y offset in pixels.
     * @param src_w Source rectangle width in pixels.
     * @param src_h Source rectangle height in pixels.
     * @param reply Output framebuffer metadata.
     * @param pixels Output tightly packed 32-bit pixels (src_w * src_h * 4 bytes).
     * @return True on success.
     */
    bool get_cursor(std::uint32_t plane_id, std::uint32_t src_x, std::uint32_t src_y, std::uint32_t src_w, std::uint32_t src_h, cursor_reply_t &reply, std::vector<std::uint8_t> &pixels);

  private:
    client_t() = default;

    bool spawn(const std::string &card_path);
    bool transact(const request_t &req, void *reply, size_t reply_size, file_t *fds_out, size_t max_fds);
    void mark_dead();

    file_t sock;
    pid_t child = -1;
    bool dead = false;
    std::mutex mtx;
  };

}  // namespace kmsp
