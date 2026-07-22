/**
 * @file src/amf/amf_d3d11.h
 * @brief Declarations for AMF D3D11 encoder.
 */
#pragma once

#include "amf_encoder.h"
#include "amf_lifecycle.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <d3d11.h>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <wrl/client.h>

#include <AMF/components/Component.h>
#include <AMF/core/Context.h>
#include <AMF/core/Data.h>
#include <AMF/core/Factory.h>

namespace amf {

  /**
   * @brief AMF encoder using D3D11 for texture input.
   */
  class amf_d3d11: public amf_encoder {
  public:
    /**
     * @brief Construct an AMF encoder around an existing D3D11 device.
     *
     * @param d3d_device D3D11 device used for conversion and AMF input.
     */
    explicit amf_d3d11(ID3D11Device *d3d_device);

    /**
     * @brief Destroy the AMF encoder and release driver resources.
     */
    ~amf_d3d11();

    bool
    create_encoder(const amf_config &config,
      const video::config_t &client_config,
      const video::sunshine_colorspace_t &colorspace,
      platf::pix_fmt_e buffer_format) override;

    void
    destroy_encoder() override;

    amf_encode_result
    encode_frame(uint64_t frame_index, bool force_idr) override;

    amf_encode_result
    drain_output(std::chrono::milliseconds timeout) override;

    bool
    begin_drain() override;

    bool
    invalidate_ref_frames(uint64_t first_frame, uint64_t last_frame) override;

    bool
    set_bitrate(int bitrate_kbps) override;

    bool
    set_hdr_metadata(const std::optional<amf_hdr_metadata> &metadata) override;

    void *
    get_input_texture() override;

    /**
     * @brief Reserve a free native texture for the next conversion.
     *
     * @return Texture that the display converter may render into, or nullptr on backpressure.
     */
    ID3D11Texture2D *
    acquire_input_texture_for_render();

    /**
     * @brief Cancel a texture reservation after conversion fails.
     */
    void
    cancel_input_texture_for_render();

  private:
    /**
     * @brief Resolve the AMF runtime and required factory functions.
     *
     * @return True when the runtime is available.
     */
    bool
    init_amf_library();

    /**
     * @brief Apply codec-independent and codec-specific AMF properties.
     *
     * @param config Native AMF settings.
     * @param client_config Negotiated stream configuration.
     * @param colorspace Output colorimetry.
     * @return True when all mandatory properties were accepted and verified.
     */
    bool
    configure_encoder(const amf_config &config,
      const video::config_t &client_config,
      const video::sunshine_colorspace_t &colorspace);

    /**
     * @brief Map Sunshine's input format to an AMF surface format.
     *
     * @param buffer_format Sunshine pixel format.
     * @param bit_depth Stream bit depth.
     * @return AMF surface format.
     */
    AMF_SURFACE_FORMAT
    get_amf_format(platf::pix_fmt_e buffer_format, int bit_depth);

    /**
     * @brief Select the AMF component identifier for the negotiated codec.
     *
     * @return AMF component identifier.
     */
    const wchar_t *
    get_codec_id();

    /**
     * @brief Copy one AMF output object into Sunshine-owned frame storage.
     *
     * @param output_data AMF output object.
     * @return Encoded frame and metadata.
     */
    amf_encoded_frame
    extract_encoded_frame(const ::amf::AMFDataPtr &output_data);

    /**
     * @brief Retrieve asynchronous AMF output until stopped or drained.
     *
     * @param stop_token Cooperative stop token.
     */
    void
    output_pump(std::stop_token stop_token) noexcept;

    /**
     * @brief Handle AMF releasing one input-surface slot.
     *
     * @param slot_index Released ring index.
     */
    void
    on_input_surface_released(std::size_t slot_index) noexcept;

    bool
    ensure_input_surface_count(std::size_t count);

    // Safe to call with state_mutex released: device and input_surface_desc are
    // immutable while the encode thread runs. Pool expansion allocates through
    // this so a multi-MB CreateTexture2D never stalls the pump or AMF's
    // release observers behind the lock.
    Microsoft::WRL::ComPtr<ID3D11Texture2D>
    create_input_surface_texture();

    ID3D11Device *device = nullptr;
    ::amf::AMFFactory *factory = nullptr;
    ::amf::AMFContextPtr context;
    ::amf::AMFComponentPtr encoder;
    HMODULE amf_dll = nullptr;

    // The converter renders directly into a reserved pool surface. AMF may retain an
    // accepted native surface while encoding it, so a slot is recycled exclusively by
    // AMFSurfaceObserver. The pool starts at the queue/lookahead-aware working depth
    // and allocates additional slots lazily during a transient driver backlog.
    static constexpr std::size_t INPUT_SURFACE_RING_SIZE = lifecycle::maximum_input_surface_count;
    using input_surface_state_e = lifecycle::input_surface_state_e;

    struct input_surface_release_observer_t final: ::amf::AMFSurfaceObserver {
      amf_d3d11 *owner = nullptr;
      std::size_t slot_index = 0;

      void AMF_STD_CALL OnSurfaceDataRelease(::amf::AMFSurface *surface) override;
    };
    struct input_surface_slot_t: lifecycle::input_surface_state_t {
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    };
    std::array<input_surface_slot_t, INPUT_SURFACE_RING_SIZE> input_surface_ring;
    std::array<input_surface_release_observer_t, INPUT_SURFACE_RING_SIZE> input_surface_release_observers;
    std::size_t active_input_surface_count = lifecycle::minimum_input_surface_count;
    std::size_t encoder_input_queue_size = lifecycle::default_amf_input_queue_size;
    D3D11_TEXTURE2D_DESC input_surface_desc {};
    std::size_t next_input_surface_slot = 0;
    std::optional<std::size_t> prepared_input_surface_slot;
    std::optional<std::size_t> last_rendered_input_surface_slot;

    // Encoder state
    video::config_t current_config {};
    int video_format = 0;  // 0=H264, 1=HEVC, 2=AV1
    AMF_SURFACE_FORMAT surface_format = AMF_SURFACE_NV12;
    int encode_width = 0;
    int encode_height = 0;
    bool rfi_pending = false;
    uint64_t last_rfi_ltr_index = 0;
    int max_ltr_frames = 0;
    bool rfi_enabled = false;

    // Current LTR state for RFI.
    // Slot 0 is reserved as the IDR baseline (set on every IDR, never overwritten by
    // periodic marks) so RFI always has a known-good fallback even when every recent
    // periodic-marked frame was inside a packet-loss window. Slots 1..N-1 form a
    // sliding window of more recent anchors.
    static constexpr int MAX_LTR_SLOTS = 4;
    static constexpr uint64_t LTR_MARK_INTERVAL = 4;  // Mark LTR every N frames
    int effective_ltr_slots = 0;    // Clamped to min(max_ltr_frames, MAX_LTR_SLOTS)
    int current_ltr_slot = 0;      // Which LTR slot to mark next
    std::array<bool, MAX_LTR_SLOTS> ltr_slots_valid {};
    std::array<uint64_t, MAX_LTR_SLOTS> ltr_slot_frame_index {};  // Frame index when each LTR slot was marked

    // QueryOutput is owned by a dedicated pump. This keeps AMF driver completion work
    // off Sunshine's latency-critical encode thread and gives input-slot ownership one
    // synchronization boundary.
    std::mutex state_mutex;
    std::condition_variable state_cv;
    std::jthread output_thread;
    std::deque<amf_encoded_frame> completed_outputs;
    std::unordered_map<uint64_t, bool> frame_rfi_flags;
    uint64_t last_completed_frame_index = 0;
    uint64_t last_submitted_frame_index = 0;
    bool output_fatal = false;
    bool drain_requested = false;
    bool drain_complete = false;
    bool output_poll_requested = false;
    std::size_t active_output_poll_waiters = 0;

    // Input ownership and output production are deliberately tracked separately.
    // AMF explicitly does not guarantee a one-to-one input/output relationship;
    // AMFSurfaceObserver owns the first count while QueryOutput owns the totals.
    std::size_t input_surfaces_in_flight = 0;
    uint64_t accepted_input_count = 0;
    uint64_t completed_output_count = 0;
    bool preanalysis_enabled = false;
    int preanalysis_lookahead_depth = 0;
    bool query_timeout_supported = false;
    bool user_configured_rate_control = false;
    bool enforce_hrd_enabled = false;

    // Statistics feedback state
    bool statistics_enabled = false;
    bool psnr_enabled = false;
    bool ssim_enabled = false;

    // Runtime fault watchdog: count consecutive failures so we can signal
    // a fatal error to the upper layer (triggering a real reinit) instead
    // of silently producing no output forever. Threshold is derived from
    // client framerate in create_encoder() so the watchdog fires after
    // roughly the same wall-clock time regardless of fps.
    int consecutive_submit_failures = 0;
    int consecutive_surface_failures = 0;
    int consecutive_query_failures = 0;
    int consecutive_output_failures = 0;
    // Consecutive submissions whose bounded coalescing target was not reached.
    int consecutive_catchup_misses = 0;
    uint64_t catchup_batch_count = 0;
    int max_consecutive_failures = 60;  // Set to ~1s of frames in create_encoder()
    std::chrono::steady_clock::time_point last_output_progress {};
    std::chrono::steady_clock::time_point submit_backpressure_started {};

    std::string last_error_string;
  };

  /**
   * @brief Create an AMF D3D11 encoder instance.
   * @param d3d_device The D3D11 device to use.
   * @return AMF encoder or nullptr on failure.
   */
  std::unique_ptr<amf_d3d11>
  create_amf_d3d11(ID3D11Device *d3d_device);

}  // namespace amf
