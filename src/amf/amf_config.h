/**
 * @file src/amf/amf_config.h
 * @brief Declarations for AMF encoder configuration.
 */
#pragma once

#include <cstdint>
#include <optional>

namespace amf {

  /**
   * @brief HDR metadata for AMF encoder.
   */
  struct amf_hdr_metadata {
    struct {
      uint16_t x;  ///< Chromaticity x coordinate normalized to 50,000.
      uint16_t y;  ///< Chromaticity y coordinate normalized to 50,000.
    } displayPrimaries[3];  ///< RGB display-primary coordinates.

    struct {
      uint16_t x;  ///< Chromaticity x coordinate normalized to 50,000.
      uint16_t y;  ///< Chromaticity y coordinate normalized to 50,000.
    } whitePoint;  ///< Display white-point coordinates.

    uint16_t maxDisplayLuminance;  ///< Maximum mastering-display luminance in nits.
    uint16_t minDisplayLuminance;  ///< Minimum mastering-display luminance in 1/10000 nit.
    uint16_t maxContentLightLevel;  ///< Maximum content light level in nits.
    uint16_t maxFrameAverageLightLevel;  ///< Maximum frame-average light level in nits.
  };

  /**
   * @brief AMF encoder configuration.
   * Integer values correspond directly to AMF SDK enum values.
   */
  struct amf_config {
    std::optional<int> usage;  ///< AMF usage-preset enum value.

    std::optional<int> quality_preset;  ///< AMF quality-preset enum value.

    std::optional<int> rc_mode;  ///< AMF rate-control method enum value.

    std::optional<int> preanalysis;  ///< Explicit PreAnalysis state, or driver default.

    std::optional<int> vbaq;  ///< Variance-based adaptive quantization state.

    std::optional<int> h264_cabac;  ///< H.264 coder: zero CAVLC, one CABAC, or driver default.

    std::optional<int> enforce_hrd;  ///< HRD-enforcement state.

    // Number of LTR frames for RFI (0 = disabled, matches FFmpeg amfenc behavior).
    // When enabled, static screen regions may retain encoder artifacts from the
    // baseline LTR frame until motion forces a refresh; only opt in when the
    // network actually needs reference-frame invalidation recovery.
    int max_ltr_frames = 0;  ///< Requested LTR slots; zero disables native RFI.

    // --- Pre-Analysis sub-system ---
    std::optional<int> pa_paq_mode;  ///< PAQ mode: zero none, one CAQ.
    std::optional<int> pa_taq_mode;  ///< TAQ mode: zero none, one mode 1, two mode 2.
    std::optional<int> pa_caq_strength;  ///< CAQ strength: zero low, one medium, two high.
    std::optional<int> pa_lookahead_depth;  ///< Lookahead depth, where zero disables lookahead.
    std::optional<int> pa_scene_change_sensitivity;  ///< Scene-change sensitivity enum value.
    std::optional<int> pa_high_motion_quality_boost;  ///< PreAnalysis high-motion boost enum value.
    std::optional<int> pa_initial_qp_after_scene_change;  ///< Initial scene-change QP from 0 to 51.
    std::optional<int> pa_activity_type;  ///< Activity type: zero luma, one YUV.

    // --- QVBR quality level ---
    std::optional<int> qvbr_quality_level;  ///< QVBR level from 1 (lowest quality) to 51 (highest).

    // --- Multi-HW instance encode / Smart Access Video ---
    // Default nullopt = do not set the property, let the driver decide.
    // H.264 exposes only Smart Access Video; HEVC/AV1 expose both SAV and
    // explicit multi-HW instance encode properties.
    std::optional<bool> multi_hw_instance_encode;  ///< Multi-instance and Smart Access Video override.

    // --- AV1 Encoding Latency Mode ---
    // AMF_VIDEO_ENCODER_AV1_ENCODING_LATENCY_MODE_ENUM: 0=none, 1=power saving RT, 2=RT, 3=lowest latency
    std::optional<int> av1_encoding_latency_mode;  ///< AMF AV1 encoding-latency enum value.

    // --- AV1 Screen Content Tools ---
    std::optional<bool> av1_screen_content_tools;  ///< AV1 screen-content tools override.
    std::optional<bool> av1_palette_mode;  ///< AV1 palette-mode override.
    std::optional<bool> av1_force_integer_mv;  ///< AV1 integer-motion-vector override.

    // --- Intra Refresh ---
    // H.264: number of MBs per slot; HEVC: number of CTBs per slot; AV1: mode enum
    std::optional<int> intra_refresh_mbs;  ///< H.264 MB or HEVC CTB intra-refresh count.
    // AV1-specific: intra refresh mode (AMF_VIDEO_ENCODER_AV1_INTRA_REFRESH_MODE_ENUM)
    std::optional<int> av1_intra_refresh_mode;  ///< AV1 intra-refresh mode enum value.
    // AV1-specific: number of stripes for intra refresh
    std::optional<int> av1_intra_refresh_stripes;  ///< AV1 intra-refresh stripe count.

    // --- Statistics feedback ---
    bool enable_statistics_feedback = false;  ///< Request native encode statistics.
    bool enable_psnr_feedback = false;  ///< Request PSNR feedback.
    bool enable_ssim_feedback = false;  ///< Request SSIM feedback.

    // --- High Motion Quality Boost (encoder-level, separate from PA) ---
    // Default nullopt = do not set the property, let the AMD driver decide
    // (FFmpeg amfenc.c never sets this property either). Some AMD driver
    // releases (e.g. Adrenalin 26.5.x on RDNA4) appear to expose latent VCN
    // bugs when this is enabled, leading to encoder freezes after ~minutes.
    std::optional<bool> high_motion_quality_boost_enable;  ///< Encoder-level high-motion boost override.

    // --- Low Latency Mode (encoder-level) ---
    // Default nullopt = do not set the property, let the driver default decide.
    // Matches FFmpeg amfenc behavior (only set when user opts in or Smart
    // Access Video is enabled). Streaming workloads usually want this true,
    // but exposing it lets users disable it as a workaround for driver bugs.
    std::optional<bool> lowlatency_mode;  ///< H.264/HEVC low-latency property override.

    // --- Input Queue Size (async_depth) ---
    // Default nullopt = do not set the property; AMD driver default ~16,
    // matches FFmpeg amfenc default. Sunshine historically forced 1 for
    // minimum latency, but that is the most fragile code path inside the
    // driver. Users can opt-in to 1 for absolute lowest latency or larger
    // values (4/8/16) as a workaround for driver freezes.
    std::optional<int> input_queue_size;  ///< AMF input queue override.
  };

}  // namespace amf
