/**
 * @file src/amf/amf_d3d11.cpp
 * @brief Implementation of standalone AMF encoder with D3D11 texture input.
 */

#include "amf_d3d11.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>

#include <AMF/components/ColorSpace.h>
#include <AMF/components/ComponentCaps.h>
#include <AMF/components/PreAnalysis.h>
#include <AMF/components/VideoEncoderAV1.h>
#include <AMF/components/VideoEncoderHEVC.h>
#include <AMF/components/VideoEncoderVCE.h>
#include <AMF/core/Surface.h>

#include "src/config.h"
#include "src/logging.h"
#include "src/utility.h"

namespace amf {

  // AMF DLL function types
  typedef AMF_RESULT(AMF_CDECL_CALL *AMFInit_Fn)(amf_uint64 version, ::amf::AMFFactory **ppFactory);
  typedef AMF_RESULT(AMF_CDECL_CALL *AMFQueryVersion_Fn)(amf_uint64 *pVersion);

  amf_d3d11::amf_d3d11(ID3D11Device *d3d_device):
      device(d3d_device) {
    for (std::size_t i = 0; i < input_surface_release_observers.size(); ++i) {
      input_surface_release_observers[i].owner = this;
      input_surface_release_observers[i].slot_index = i;
    }
  }

  amf_d3d11::~amf_d3d11() {
    destroy_encoder();
  }

  void AMF_STD_CALL
  amf_d3d11::input_surface_release_observer_t::OnSurfaceDataRelease(::amf::AMFSurface *surface) {
    (void) surface;
    if (owner) {
      owner->on_input_surface_released(slot_index);
    }
  }

  void
  amf_d3d11::on_input_surface_released(std::size_t slot_index) noexcept {
    std::lock_guard lock(state_mutex);
    if (slot_index >= input_surface_ring.size()) {
      return;
    }

    auto &slot = input_surface_ring[slot_index];
    if (lifecycle::on_surface_released(slot)) {
      if (input_surfaces_in_flight > 0) {
        --input_surfaces_in_flight;
      }
      state_cv.notify_all();
    }
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D>
  amf_d3d11::create_input_surface_texture() {
    static const GUID AMFTextureArrayIndexGUID = { 0x28115527, 0xe7c3, 0x4b66, { 0x99, 0xd3, 0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf } };
    int array_index = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    const auto hr = device->CreateTexture2D(
      &input_surface_desc,
      nullptr,
      texture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
      BOOST_LOG(error) << "AMF: failed to create direct-render input texture"
                       << ", HRESULT: 0x" << std::hex << hr;
      return nullptr;
    }
    texture->SetPrivateData(AMFTextureArrayIndexGUID, sizeof(array_index), &array_index);
    return texture;
  }

  bool
  amf_d3d11::ensure_input_surface_count(std::size_t count) {
    count = std::min(count, input_surface_ring.size());
    for (std::size_t index = 0; index < count; ++index) {
      auto &slot = input_surface_ring[index];
      if (slot.texture) {
        continue;
      }
      slot.texture = create_input_surface_texture();
      if (!slot.texture) {
        return false;
      }
    }
    return true;
  }

  bool
  amf_d3d11::init_amf_library() {
    if (factory) return true;

    amf_dll = LoadLibraryA(AMF_DLL_NAMEA);
    if (!amf_dll) {
      BOOST_LOG(error) << "AMF: failed to load " << AMF_DLL_NAMEA;
      return false;
    }

    auto amf_query_version = reinterpret_cast<AMFQueryVersion_Fn>(GetProcAddress(amf_dll, AMF_QUERY_VERSION_FUNCTION_NAME));
    auto amf_init = reinterpret_cast<AMFInit_Fn>(GetProcAddress(amf_dll, AMF_INIT_FUNCTION_NAME));

    if (!amf_query_version || !amf_init) {
      BOOST_LOG(error) << "AMF: missing entry points in " << AMF_DLL_NAMEA;
      FreeLibrary(amf_dll);
      amf_dll = nullptr;
      return false;
    }

    amf_uint64 version = 0;
    if (amf_query_version(&version) != AMF_OK) {
      BOOST_LOG(error) << "AMF: failed to query runtime version";
      FreeLibrary(amf_dll);
      amf_dll = nullptr;
      return false;
    }

    BOOST_LOG(info) << "AMF runtime version: "
                    << AMF_GET_MAJOR_VERSION(version) << "."
                    << AMF_GET_MINOR_VERSION(version) << "."
                    << AMF_GET_SUBMINOR_VERSION(version) << "."
                    << AMF_GET_BUILD_VERSION(version);

    if (amf_init(AMF_FULL_VERSION, &factory) != AMF_OK || !factory) {
      BOOST_LOG(error) << "AMF: AMFInit failed";
      FreeLibrary(amf_dll);
      amf_dll = nullptr;
      return false;
    }

    return true;
  }

  AMF_SURFACE_FORMAT
  amf_d3d11::get_amf_format(platf::pix_fmt_e buffer_format, int bit_depth) {
    switch (buffer_format) {
      case platf::pix_fmt_e::nv12:
        return AMF_SURFACE_NV12;
      case platf::pix_fmt_e::p010:
        return AMF_SURFACE_P010;
      default:
        return (bit_depth == 10) ? AMF_SURFACE_P010 : AMF_SURFACE_NV12;
    }
  }

  const wchar_t *
  amf_d3d11::get_codec_id() {
    switch (video_format) {
      case 0:
        return AMFVideoEncoderVCE_AVC;
      case 1:
        return AMFVideoEncoder_HEVC;
      case 2:
        return AMFVideoEncoder_AV1;
      default:
        return AMFVideoEncoderVCE_AVC;
    }
  }

  bool
  amf_d3d11::configure_encoder(const amf_config &config,
    const video::config_t &client_config,
    const video::sunshine_colorspace_t &colorspace) {
    auto bitrate = static_cast<int64_t>(client_config.bitrate) * 1000;
    AVRational fps {client_config.framerate > 0 ? client_config.framerate : 60, 1};
    auto framerate = AMFConstructRate(fps.num, fps.den);
    // Cap the VBV/HRD buffer at ~1 frame of bits, matching FFmpeg's amfenc
    // (rc_buffer_size = bitrate / framerate). Left at the AMF default (~1 second of
    // bitrate) a single IDR or scene-change frame can balloon enormous - big enough
    // to overflow the stream FEC block limit (stream.cpp MAX_FEC_BLOCKS = 4). Such
    // frames are then sent WITHOUT FEC, and losing one makes the client request an
    // IDR that is also oversized, cascading into multi-second freezes (observed on
    // RDNA4 / RX 9070 XT at 200 Mbps + 165 fps: "Skipping FEC for abnormally large
    // encoded frame"). A ~1-frame buffer keeps every frame comfortably FEC-sized.
    const int64_t vbv_buffer_size = fps.num > 0 ? (bitrate * fps.den / fps.num) : bitrate;
    // Match FFmpeg's default AMF path: set only target bitrate unless the user
    // explicitly selects a rate-control mode. In that opt-in path, keep the
    // legacy Sunshine peak/VBV constraints paired with the selected RC mode.
    user_configured_rate_control = config.rc_mode.has_value();
    enforce_hrd_enabled = config.enforce_hrd && *config.enforce_hrd;

    ::amf::AMFCapsPtr encoder_caps;
    const auto caps_result = encoder->GetCaps(&encoder_caps);
    if (caps_result != AMF_OK || !encoder_caps) {
      BOOST_LOG(warning) << "AMF: failed to query encoder capabilities, error: " << caps_result;
    }

    max_ltr_frames = 0;
    rfi_enabled = false;
    preanalysis_enabled = false;
    preanalysis_lookahead_depth = 0;

    const auto preanalysis_plan = lifecycle::resolve_preanalysis(config.rc_mode, config.preanalysis);
    const bool adaptive_quantization_supported =
      lifecycle::rate_control_supports_adaptive_quantization(config.rc_mode);
    if (!adaptive_quantization_supported && config.vbaq && *config.vbaq) {
      BOOST_LOG(info) << "AMF: disabling adaptive quantization because CQP rate control is selected";
    }
    const wchar_t *rate_control_property = video_format == 0 ? AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD :
                                           video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD :
                                                               AMF_VIDEO_ENCODER_AV1_RATE_CONTROL_METHOD;
    const wchar_t *preanalysis_property = video_format == 0 ? AMF_VIDEO_ENCODER_PRE_ANALYSIS_ENABLE :
                                          video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_PRE_ANALYSIS_ENABLE :
                                                              AMF_VIDEO_ENCODER_AV1_PRE_ANALYSIS_ENABLE;
    const wchar_t *preanalysis_capability = video_format == 0 ? AMF_VIDEO_ENCODER_CAP_PRE_ANALYSIS :
                                            video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_CAP_PRE_ANALYSIS :
                                                                AMF_VIDEO_ENCODER_AV1_CAP_PRE_ANALYSIS;

    if (preanalysis_plan.enabled && encoder_caps) {
      amf_bool supported = false;
      const auto capability_result = encoder_caps->GetProperty(preanalysis_capability, &supported);
      if (capability_result == AMF_OK && !supported) {
        BOOST_LOG(warning) << "AMF: PreAnalysis is required by the requested configuration but unsupported by this encoder";
        return false;
      }
    }

    auto set_verified_int64 = [&](const wchar_t *property, amf_int64 requested, const char *label) {
      const auto set_result = encoder->SetProperty(property, requested);
      amf_int64 applied = 0;
      const auto get_result = encoder->GetProperty(property, &applied);
      if (set_result != AMF_OK || get_result != AMF_OK || applied != requested) {
        BOOST_LOG(warning) << "AMF: failed to apply " << label << " (requested=" << requested
                           << ", applied=" << applied << ", set=" << set_result
                           << ", get=" << get_result << ')';
        return false;
      }
      return true;
    };
    auto set_verified_bool = [&](const wchar_t *property, bool requested, const char *label) {
      const auto set_result = encoder->SetProperty(property, requested);
      amf_bool applied = false;
      const auto get_result = encoder->GetProperty(property, &applied);
      if (set_result != AMF_OK || get_result != AMF_OK || static_cast<bool>(applied) != requested) {
        BOOST_LOG(warning) << "AMF: failed to apply " << label << " (requested=" << requested
                           << ", applied=" << static_cast<bool>(applied) << ", set=" << set_result
                           << ", get=" << get_result << ')';
        return false;
      }
      return true;
    };
    auto set_required = [&](const wchar_t *property, const auto &requested, const char *label) {
      const auto set_result = encoder->SetProperty(property, requested);
      if (set_result != AMF_OK) {
        BOOST_LOG(warning) << "AMF: failed to set required " << label << " property (error=" << set_result << ')';
        return false;
      }
      return true;
    };

    auto configure_reference_frames = [&](const wchar_t *property) {
      if (client_config.numRefFrames <= 0) {
        return true;
      }

      const auto requested = static_cast<amf_int64>(client_config.numRefFrames);
      const auto set_result = encoder->SetProperty(property, requested);
      amf_int64 applied = 0;
      const auto get_result = encoder->GetProperty(property, &applied);
      if (set_result != AMF_OK || get_result != AMF_OK || applied != requested) {
        BOOST_LOG(warning) << "AMF: failed to apply the client reference-frame limit (requested="
                           << requested << ", applied=" << applied << ", set=" << set_result
                           << ", get=" << get_result << ')';
        return false;
      }
      return true;
    };

    auto configure_ltr = [&](const wchar_t *max_frames_property,
                             const wchar_t *mode_property,
                             amf_int64 reset_unused_mode,
                             const wchar_t *max_frames_capability) {
      if (config.max_ltr_frames <= 0) {
        return;
      }

      int supported_frames = std::min(config.max_ltr_frames, MAX_LTR_SLOTS);

      // Property metadata is available for AVC/HEVC even though those codecs do
      // not expose a named AMFCaps maximum. AV1 additionally exposes an explicit
      // capability; clamp to whichever driver limit is available.
      const ::amf::AMFPropertyInfo *property_info = nullptr;
      if (encoder->GetPropertyInfo(max_frames_property, &property_info) == AMF_OK &&
          property_info && property_info->maxValue.type == AMF_VARIANT_INT64 &&
          property_info->maxValue.int64Value > 0) {
        supported_frames = std::min(supported_frames, static_cast<int>(property_info->maxValue.int64Value));
      }
      if (max_frames_capability && encoder_caps) {
        amf_int64 capability_limit = 0;
        if (encoder_caps->GetProperty(max_frames_capability, &capability_limit) == AMF_OK && capability_limit >= 0) {
          supported_frames = std::min(supported_frames, static_cast<int>(capability_limit));
        }
      }

      if (supported_frames <= 0) {
        BOOST_LOG(warning) << "AMF: LTR requested, but the driver reports no available LTR slots; disabling RFI";
        return;
      }

      const auto max_result = encoder->SetProperty(max_frames_property, static_cast<amf_int64>(supported_frames));
      const auto mode_result = encoder->SetProperty(mode_property, reset_unused_mode);
      amf_int64 applied_frames = 0;
      const auto readback_result = encoder->GetProperty(max_frames_property, &applied_frames);
      if (max_result != AMF_OK || mode_result != AMF_OK || readback_result != AMF_OK || applied_frames <= 0) {
        BOOST_LOG(warning) << "AMF: failed to configure LTR reliably (max=" << max_result
                           << ", mode=" << mode_result << ", readback=" << readback_result
                           << "); disabling RFI";
        // Best effort: avoid reserving driver LTR resources when Sunshine has
        // disabled the feature locally because setup was not trustworthy.
        encoder->SetProperty(max_frames_property, static_cast<amf_int64>(0));
        return;
      }

      max_ltr_frames = std::min({supported_frames, static_cast<int>(applied_frames), MAX_LTR_SLOTS});
      rfi_enabled = max_ltr_frames > 0;
      if (max_ltr_frames != config.max_ltr_frames) {
        BOOST_LOG(info) << "AMF: clamped requested LTR slots from " << config.max_ltr_frames
                        << " to driver-supported value " << max_ltr_frames;
      }
    };

    auto configure_multi_hw_instance = [&](const wchar_t *multi_hw_property,
                                           const wchar_t *sav_property,
                                           const wchar_t *hw_instances_cap,
                                           const wchar_t *sav_support_cap) {
      if (!config.multi_hw_instance_encode) return true;

      const bool enabled = *config.multi_hw_instance_encode;
      amf_int64 hw_instances = 0;
      const bool hw_cap_known = hw_instances_cap && encoder_caps && encoder_caps->GetProperty(hw_instances_cap, &hw_instances) == AMF_OK;
      amf_bool sav_supported = false;
      const bool sav_cap_known = sav_support_cap && encoder_caps && encoder_caps->GetProperty(sav_support_cap, &sav_supported) == AMF_OK;
      const bool multi_supported = multi_hw_property && hw_cap_known && hw_instances > 1;
      const bool sav_supported_by_codec = sav_property && sav_cap_known && sav_supported;

      if (enabled) {
        if (!multi_supported && !sav_supported_by_codec) {
          BOOST_LOG(warning) << "AMF: multi-HW instance encode was requested, but AMFCaps does not report support"
                             << " (instances=" << (hw_cap_known ? std::to_string(hw_instances) : "unknown")
                             << ", SAV=" << (sav_cap_known ? (sav_supported ? "yes" : "no") : "unknown") << ')';
          return false;
        }

        if (multi_supported && !set_verified_bool(multi_hw_property, true, "multi-HW instance encode")) {
          return false;
        }
        if (sav_supported_by_codec && !set_verified_bool(sav_property, true, "Smart Access Video")) {
          return false;
        }
        return true;
      }

      // A feature absent from AMFCaps is already effectively disabled. When the
      // property is supported, verify the user's explicit opt-out.
      if (multi_supported && !set_verified_bool(multi_hw_property, false, "multi-HW instance encode")) {
        return false;
      }
      if (sav_supported_by_codec && !set_verified_bool(sav_property, false, "Smart Access Video")) {
        return false;
      }
      return true;
    };

    if (video_format == 0) {
      // H.264
      if (!configure_reference_frames(AMF_VIDEO_ENCODER_MAX_NUM_REFRAMES)) return false;
      if (config.usage && !set_verified_int64(AMF_VIDEO_ENCODER_USAGE, *config.usage, "H.264 usage preset")) return false;
      if (config.quality_preset && !set_verified_int64(AMF_VIDEO_ENCODER_QUALITY_PRESET, *config.quality_preset, "H.264 quality preset")) return false;
      if (!set_verified_int64(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH, "H.264 profile")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitrate, "H.264 target bitrate")) return false;
      if (user_configured_rate_control) {
        if (!set_required(AMF_VIDEO_ENCODER_PEAK_BITRATE, bitrate, "H.264 peak bitrate")) return false;
        if (!set_required(AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, vbv_buffer_size, "H.264 VBV buffer size")) return false;
      }
      if (!set_required(AMF_VIDEO_ENCODER_FRAMERATE, framerate, "H.264 frame rate")) return false;
      if (config.enforce_hrd) {
        if (!set_verified_bool(AMF_VIDEO_ENCODER_ENFORCE_HRD, !!(*config.enforce_hrd), "H.264 HRD enforcement")) return false;
        // Belt-and-braces with HRD: hard-cap the peak access-unit size so no single frame
        // (IDR / scene change) can overrun the stream FEC budget at high bitrate. ~4x the
        // per-frame VBV budget leaves normal IDRs intact while stopping the runaway frames
        // that froze RDNA4 at 200+ Mbps. Only applied when HRD enforcement is opted in.
        if (*config.enforce_hrd &&
            !set_verified_int64(AMF_VIDEO_ENCODER_MAX_AU_SIZE, vbv_buffer_size * 4, "H.264 maximum access-unit size")) return false;
      }
      if (!set_required(AMF_VIDEO_ENCODER_IDR_PERIOD, (amf_int64) 0, "H.264 infinite IDR period")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_DE_BLOCKING_FILTER, true, "H.264 deblocking filter")) return false;
      if (config.h264_cabac) {
        if (!set_verified_int64(
              AMF_VIDEO_ENCODER_CABAC_ENABLE,
              static_cast<amf_int64>(*config.h264_cabac ? AMF_VIDEO_ENCODER_CABAC : AMF_VIDEO_ENCODER_CALV),
              "H.264 entropy coder")) {
          return false;
        }
      }
      if ((config.vbaq || !adaptive_quantization_supported) &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_ENABLE_VBAQ,
            adaptive_quantization_supported && config.vbaq && !!(*config.vbaq),
            "H.264 VBAQ")) return false;
      encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, (amf_int64) 0);
      // LOWLATENCY_MODE and INPUT_QUEUE_SIZE: only set when user opts in.
      // Matches FFmpeg amfenc behavior (FFmpeg never forces these properties).
      // Forcing them to true/1 has been observed to expose latent AMD driver
      // bugs (see AlkaidLab/foundation-sunshine#666 freeze on RDNA4 26.5.x).
      if (config.lowlatency_mode && !set_verified_bool(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, *config.lowlatency_mode, "H.264 low-latency mode")) return false;
      if (config.input_queue_size && !set_verified_int64(AMF_VIDEO_ENCODER_INPUT_QUEUE_SIZE, *config.input_queue_size, "H.264 input queue size")) return false;
      if (!configure_multi_hw_instance(
            nullptr,
            AMF_VIDEO_ENCODER_ENABLE_SMART_ACCESS_VIDEO,
            AMF_VIDEO_ENCODER_CAP_NUM_OF_HW_INSTANCES,
            AMF_VIDEO_ENCODER_CAP_SUPPORT_SMART_ACCESS_VIDEO)) {
        return false;
      }
      encoder->SetProperty(AMF_VIDEO_ENCODER_QUERY_TIMEOUT, (amf_int64) 1);

      // LTR for RFI (Reference Frame Invalidation, weak-network recovery).
      //
      // Disabled by default (max_ltr_frames == 0) to match FFmpeg amfenc behavior:
      // FFmpeg's libavcodec/amfenc.c never sets MAX_LTR_FRAMES / LTR_MODE, so static
      // screen regions are not pinned to a baseline LTR frame and never accumulate
      // color-block artifacts.
      //
      // Trade-off when the user opts in (amd_ltr_frames >= 1):
      //   + On lossy links, client-side reference invalidation can recover by
      //     sending a P-frame referencing a known-good LTR slot instead of a full
      //     IDR. IDRs are 10-20x larger than P-frames and themselves more likely
      //     to be lost on weak networks, which can cascade into an "IDR storm".
      //   - Static regions may inherit the IDR-time quantization noise of the
      //     baseline LTR slot until motion forces a fresh intra block.
      //
      // The slot rotation / IDR-baseline preservation logic below (PR #630) only
      // takes effect when LTR is opted in.
      configure_ltr(
        AMF_VIDEO_ENCODER_MAX_LTR_FRAMES,
        AMF_VIDEO_ENCODER_LTR_MODE,
        AMF_VIDEO_ENCODER_LTR_MODE_RESET_UNUSED,
        nullptr);

      // High motion quality boost
      if (config.high_motion_quality_boost_enable &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_HIGH_MOTION_QUALITY_BOOST_ENABLE,
            *config.high_motion_quality_boost_enable,
            "H.264 high-motion quality boost")) return false;

      // Intra refresh
      if (config.intra_refresh_mbs &&
          !set_verified_int64(
            AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT,
            *config.intra_refresh_mbs,
            "H.264 intra-refresh macroblocks")) return false;

      // Slices per frame
      if (client_config.slicesPerFrame > 1 &&
          !set_verified_int64(AMF_VIDEO_ENCODER_SLICES_PER_FRAME, client_config.slicesPerFrame, "H.264 slices per frame")) return false;

      // Statistics feedback is a per-submission surface property. It is applied to
      // sampled input surfaces in encode_frame(), never to the encoder component.
    }
    else if (video_format == 1) {
      // HEVC
      if (!configure_reference_frames(AMF_VIDEO_ENCODER_HEVC_MAX_NUM_REFRAMES)) return false;
      if (config.usage && !set_verified_int64(AMF_VIDEO_ENCODER_HEVC_USAGE, *config.usage, "HEVC usage preset")) return false;
      if (config.quality_preset && !set_verified_int64(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, *config.quality_preset, "HEVC quality preset")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitrate, "HEVC target bitrate")) return false;
      if (user_configured_rate_control) {
        if (!set_required(AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE, bitrate, "HEVC peak bitrate")) return false;
        if (!set_required(AMF_VIDEO_ENCODER_HEVC_VBV_BUFFER_SIZE, vbv_buffer_size, "HEVC VBV buffer size")) return false;
      }
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, framerate, "HEVC frame rate")) return false;
      if (config.enforce_hrd) {
        if (!set_verified_bool(AMF_VIDEO_ENCODER_HEVC_ENFORCE_HRD, !!(*config.enforce_hrd), "HEVC HRD enforcement")) return false;
        // See H.264 above: cap the peak AU size (~4x per-frame VBV) so no frame overruns FEC.
        if (*config.enforce_hrd &&
            !set_verified_int64(AMF_VIDEO_ENCODER_HEVC_MAX_AU_SIZE, vbv_buffer_size * 4, "HEVC maximum access-unit size")) return false;
      }
      // HEADER_INSERTION_MODE is deliberately left at the driver default (NONE),
      // matching FFmpeg's hevc_amf (stable on the same cards). Forcing IDR_ALIGNED
      // was the last unconditional HEVC-only divergence from the paths that work on
      // RDNA4, where native HEVC kept freezing. Headers still reach the client:
      // every force_idr frame sets HEVC_INSERT_HEADER below, so VPS/SPS/PPS ride on
      // each IDR exactly like the FFmpeg path.
      // Infinite GOP (no periodic IDR), matching both FFmpeg's hevc_amf path (which
      // uses an infinite GOP - see video.cpp "infinite GOP length") and the native AV1
      // path. Keyframes are client-driven via force_idr (initial frame + packet-loss
      // recovery). A periodic IDR (GOP 60) emitted a large keyframe every 60 frames;
      // at high bitrate those overflow the stream FEC block limit (MAX_FEC_BLOCKS = 4,
      // stream.cpp) and ship unprotected, so a single loss cascades into a freeze. A
      // tester's host log proved it directly: in one session, GOP-60 HEVC produced 319
      // over-FEC frames while GOP-0 AV1 produced 0 on the same RX 9070 XT. (An earlier
      // note that GOP 0 misbehaves on RDNA4 is contradicted by AV1 and FFmpeg both
      // running infinite-GOP fine on that card.)
      //
      // GOP_SIZE=0 alone gives the infinite GOP. We deliberately do NOT set
      // NUM_GOPS_PER_IDR: the native AV1 path (rock-solid on RDNA4 up to 500 Mbps) sets
      // only GOP_SIZE, and NUM_GOPS_PER_IDR=0 is a degenerate value ("zero GOPs per
      // IDR") that RDNA4's HEVC VCN mishandles - multiple RX 9070 XT testers froze on
      // native HEVC (freeze on the first keyframe need) while native AV1 and FFmpeg
      // hevc_amf, neither of which sets it, ran clean on the same cards. force_idr
      // keyframes are driven per-surface via HEVC_FORCE_PICTURE_TYPE, independent of this.
      encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_GOP_SIZE, (amf_int64) 0);
      if ((config.vbaq || !adaptive_quantization_supported) &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_HEVC_ENABLE_VBAQ,
            adaptive_quantization_supported && config.vbaq && !!(*config.vbaq),
            "HEVC VBAQ")) return false;
      // LOWLATENCY_MODE and INPUT_QUEUE_SIZE: only set when user opts in.
      // See H.264 block above for rationale (FFmpeg-aligned default behavior).
      if (config.lowlatency_mode && !set_verified_bool(AMF_VIDEO_ENCODER_HEVC_LOWLATENCY_MODE, *config.lowlatency_mode, "HEVC low-latency mode")) return false;
      if (config.input_queue_size && !set_verified_int64(AMF_VIDEO_ENCODER_HEVC_INPUT_QUEUE_SIZE, *config.input_queue_size, "HEVC input queue size")) return false;
      if (!configure_multi_hw_instance(
            AMF_VIDEO_ENCODER_HEVC_MULTI_HW_INSTANCE_ENCODE,
            AMF_VIDEO_ENCODER_HEVC_ENABLE_SMART_ACCESS_VIDEO,
            AMF_VIDEO_ENCODER_HEVC_CAP_NUM_OF_HW_INSTANCES,
            AMF_VIDEO_ENCODER_HEVC_CAP_SUPPORT_SMART_ACCESS_VIDEO)) {
        return false;
      }
      encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUERY_TIMEOUT, (amf_int64) 1);

      if (colorspace.bit_depth == 10) {
        if (!set_required(AMF_VIDEO_ENCODER_HEVC_PROFILE, (amf_int64) AMF_VIDEO_ENCODER_HEVC_PROFILE_MAIN_10, "HEVC Main10 profile")) return false;
      }
      else {
        if (!set_required(AMF_VIDEO_ENCODER_HEVC_PROFILE, (amf_int64) AMF_VIDEO_ENCODER_HEVC_PROFILE_MAIN, "HEVC Main profile")) return false;
      }

      // LTR for RFI - see H.264 block above for detailed trade-off rationale.
      // Disabled by default; opt-in via amd_ltr_frames config.
      configure_ltr(
        AMF_VIDEO_ENCODER_HEVC_MAX_LTR_FRAMES,
        AMF_VIDEO_ENCODER_HEVC_LTR_MODE,
        AMF_VIDEO_ENCODER_HEVC_LTR_MODE_RESET_UNUSED,
        nullptr);

      // High motion quality boost
      if (config.high_motion_quality_boost_enable &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_HEVC_HIGH_MOTION_QUALITY_BOOST_ENABLE,
            *config.high_motion_quality_boost_enable,
            "HEVC high-motion quality boost")) return false;

      // Intra refresh
      if (config.intra_refresh_mbs &&
          !set_verified_int64(
            AMF_VIDEO_ENCODER_HEVC_INTRA_REFRESH_NUM_CTBS_PER_SLOT,
            *config.intra_refresh_mbs,
            "HEVC intra-refresh CTBs")) return false;

      // Slices per frame
      if (client_config.slicesPerFrame > 1 &&
          !set_verified_int64(AMF_VIDEO_ENCODER_HEVC_SLICES_PER_FRAME, client_config.slicesPerFrame, "HEVC slices per frame")) return false;

      // Statistics feedback is applied per input surface in encode_frame().
    }
    else {
      // AV1
      if (!configure_reference_frames(AMF_VIDEO_ENCODER_AV1_MAX_NUM_REFRAMES)) return false;
      if (config.usage && !set_verified_int64(AMF_VIDEO_ENCODER_AV1_USAGE, *config.usage, "AV1 usage preset")) return false;
      if (config.quality_preset && !set_verified_int64(AMF_VIDEO_ENCODER_AV1_QUALITY_PRESET, *config.quality_preset, "AV1 quality preset")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_AV1_TARGET_BITRATE, bitrate, "AV1 target bitrate")) return false;
      if (user_configured_rate_control) {
        if (!set_required(AMF_VIDEO_ENCODER_AV1_PEAK_BITRATE, bitrate, "AV1 peak bitrate")) return false;
        if (!set_required(AMF_VIDEO_ENCODER_AV1_VBV_BUFFER_SIZE, vbv_buffer_size, "AV1 VBV buffer size")) return false;
      }
      if (!set_required(AMF_VIDEO_ENCODER_AV1_FRAMERATE, framerate, "AV1 frame rate")) return false;
      if (config.enforce_hrd) {
        if (!set_verified_bool(AMF_VIDEO_ENCODER_AV1_ENFORCE_HRD, !!(*config.enforce_hrd), "AV1 HRD enforcement")) return false;
        // See H.264 above: cap the peak compressed frame size (~4x per-frame VBV) to fit FEC.
        if (*config.enforce_hrd &&
            !set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_MAX_COMPRESSED_FRAME_SIZE,
              vbv_buffer_size * 4,
              "AV1 maximum compressed-frame size")) return false;
      }
      if (!set_required(
            AMF_VIDEO_ENCODER_AV1_ALIGNMENT_MODE,
            (amf_int64) AMF_VIDEO_ENCODER_AV1_ALIGNMENT_MODE_NO_RESTRICTIONS,
            "AV1 alignment mode")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_AV1_GOP_SIZE, (amf_int64) 0, "AV1 infinite GOP")) return false;
      // INPUT_QUEUE_SIZE / ENCODING_LATENCY_MODE: only set when user opts in.
      // Matches FFmpeg amfenc behavior (never auto-forces LOWEST_LATENCY).
      // See AlkaidLab/foundation-sunshine#666 for the RDNA4 freeze that
      // motivated stopping aggressive defaults.
      if (config.input_queue_size && !set_verified_int64(AMF_VIDEO_ENCODER_AV1_INPUT_QUEUE_SIZE, *config.input_queue_size, "AV1 input queue size")) return false;
      if (!configure_multi_hw_instance(
            AMF_VIDEO_ENCODER_AV1_MULTI_HW_INSTANCE_ENCODE,
            AMF_VIDEO_ENCODER_AV1_ENABLE_SMART_ACCESS_VIDEO,
            AMF_VIDEO_ENCODER_AV1_CAP_NUM_OF_HW_INSTANCES,
            AMF_VIDEO_ENCODER_AV1_CAP_SUPPORT_SMART_ACCESS_VIDEO)) {
        return false;
      }
      encoder->SetProperty(AMF_VIDEO_ENCODER_AV1_QUERY_TIMEOUT, (amf_int64) 1);
      if (config.av1_encoding_latency_mode) {
        if (!set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_ENCODING_LATENCY_MODE,
              *config.av1_encoding_latency_mode,
              "AV1 encoding latency mode")) return false;
      }

      // AV1 Screen Content Tools
      if (config.av1_screen_content_tools &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_AV1_SCREEN_CONTENT_TOOLS,
            *config.av1_screen_content_tools,
            "AV1 screen-content tools")) return false;
      if (config.av1_palette_mode &&
          !set_verified_bool(AMF_VIDEO_ENCODER_AV1_PALETTE_MODE, *config.av1_palette_mode, "AV1 palette mode")) return false;
      if (config.av1_force_integer_mv &&
          !set_verified_bool(AMF_VIDEO_ENCODER_AV1_FORCE_INTEGER_MV, *config.av1_force_integer_mv, "AV1 integer motion vectors")) return false;

      // AV1 high motion quality boost
      if (config.high_motion_quality_boost_enable &&
          !set_verified_bool(
            AMF_VIDEO_ENCODER_AV1_HIGH_MOTION_QUALITY_BOOST,
            *config.high_motion_quality_boost_enable,
            "AV1 high-motion quality boost")) return false;

      // The codec-unqualified amd_vbaq setting maps to AV1 content-adaptive
      // quantization. PAQ remains a fallback for callers that configure the
      // lower-level AMF API directly.
      if (!adaptive_quantization_supported) {
        if (!set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_AQ_MODE,
              AMF_VIDEO_ENCODER_AV1_AQ_MODE_NONE,
              "AV1 adaptive quantization")) return false;
      } else if (config.vbaq) {
        if (!set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_AQ_MODE,
              static_cast<amf_int64>(*config.vbaq ? AMF_VIDEO_ENCODER_AV1_AQ_MODE_CAQ : AMF_VIDEO_ENCODER_AV1_AQ_MODE_NONE),
              "AV1 adaptive quantization")) return false;
      } else if (config.pa_paq_mode) {
        if (!set_verified_int64(AMF_VIDEO_ENCODER_AV1_AQ_MODE, *config.pa_paq_mode, "AV1 PAQ mode")) return false;
      }

      // LTR for RFI - see H.264 block above for detailed trade-off rationale.
      // Disabled by default; opt-in via amd_ltr_frames config.
      configure_ltr(
        AMF_VIDEO_ENCODER_AV1_MAX_LTR_FRAMES,
        AMF_VIDEO_ENCODER_AV1_LTR_MODE,
        AMF_VIDEO_ENCODER_AV1_LTR_MODE_RESET_UNUSED,
        AMF_VIDEO_ENCODER_AV1_CAP_MAX_NUM_LTR_FRAMES);

      // Intra refresh
      if (config.av1_intra_refresh_mode) {
        if (!set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_INTRA_REFRESH_MODE,
              *config.av1_intra_refresh_mode,
              "AV1 intra-refresh mode")) return false;
        if (config.av1_intra_refresh_stripes &&
            !set_verified_int64(
              AMF_VIDEO_ENCODER_AV1_INTRAREFRESH_STRIPES,
              *config.av1_intra_refresh_stripes,
              "AV1 intra-refresh stripes")) return false;
      }

      // Tiles per frame
      if (client_config.slicesPerFrame > 1 &&
          !set_verified_int64(AMF_VIDEO_ENCODER_AV1_TILES_PER_FRAME, client_config.slicesPerFrame, "AV1 tiles per frame")) return false;

      // Statistics feedback is applied per input surface in encode_frame().
    }

    // AMD's TranscodePipeline sample sets rate control before enabling PA because
    // PA property application can otherwise fail. Keep that dependency ordering
    // and verify the complete pair. A driver that rejects either property fails
    // this native session so the upper layer can choose legacy AMF without
    // silently changing modes.
    const int requested_depth = preanalysis_plan.enabled ?
                                  std::max(1, config.pa_lookahead_depth.value_or(preanalysis_plan.lookahead_depth)) :
                                  0;
    if (!lifecycle::apply_rate_control_and_preanalysis(
          config.rc_mode,
          config.preanalysis.has_value() || preanalysis_plan.enabled,
          preanalysis_plan,
          requested_depth,
          [&](int value) {
            return set_verified_int64(rate_control_property, value, "rate-control mode");
          },
          [&](bool value) {
            return set_verified_bool(preanalysis_property, value, "PreAnalysis");
          },
          [&](int value) {
            return set_verified_int64(AMF_PA_LOOKAHEAD_BUFFER_DEPTH, value, "PreAnalysis lookahead depth");
          })) {
      return false;
    }
    if (preanalysis_plan.enabled) {
      preanalysis_enabled = true;
      preanalysis_lookahead_depth = requested_depth;
    }

    // Sunshine's legacy AMF path explicitly disables rate-control frame skipping.
    // Keep native packet/PTS semantics identical and do not let ULL usage presets
    // silently discard a frame when the bitrate controller is under pressure.
    const wchar_t *skip_frame_property = video_format == 0 ? AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE :
                                           video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_SKIP_FRAME_ENABLE :
                                                               AMF_VIDEO_ENCODER_AV1_RATE_CONTROL_SKIP_FRAME;
    if (!set_verified_bool(skip_frame_property, false, "rate-control frame skipping")) {
      return false;
    }

    if (config.qvbr_quality_level && config.rc_mode && *config.rc_mode == 4) {
      const wchar_t *qvbr_quality_property = video_format == 0 ? AMF_VIDEO_ENCODER_QVBR_QUALITY_LEVEL :
                                               video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_QVBR_QUALITY_LEVEL :
                                                                   AMF_VIDEO_ENCODER_AV1_QVBR_QUALITY_LEVEL;
      if (!set_verified_int64(
            qvbr_quality_property,
            static_cast<amf_int64>(*config.qvbr_quality_level),
            "QVBR quality level")) {
        return false;
      }
    }

    // Color space properties
    if (video_format == 0) {
      if (!set_required(AMF_VIDEO_ENCODER_FULL_RANGE_COLOR, colorspace.full_range, "H.264 nominal range")) return false;
    }
    else if (video_format == 1) {
      const auto nominal_range = (amf_int64)(colorspace.full_range ? AMF_VIDEO_ENCODER_HEVC_NOMINAL_RANGE_FULL : AMF_VIDEO_ENCODER_HEVC_NOMINAL_RANGE_STUDIO);
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_NOMINAL_RANGE, nominal_range, "HEVC nominal range")) return false;
    }
    else {
      // AV1: amf_bool type
      // NOMINAL_RANGE and OUTPUT_FULL_RANGE_COLOR share the same AMF property;
      // the former name keeps compatibility with the AMF 1.4 headers shipped by
      // older Apollo dependency bundles.
      if (!set_required(AMF_VIDEO_ENCODER_AV1_NOMINAL_RANGE, colorspace.full_range, "AV1 nominal range")) return false;
    }

    // Color properties for bitstream metadata.
    // Only set OUTPUT properties, matching FFmpeg's approach.
    // Do NOT set INPUT_COLOR_xxx — setting them may trigger AMF's internal color converter.
    amf_int64 amf_primaries;
    amf_int64 amf_transfer;
    amf_int64 amf_color_profile;

    switch (colorspace.colorspace) {
      case video::colorspace_e::rec601:
        amf_primaries = AMF_COLOR_PRIMARIES_SMPTE170M;
        amf_transfer = AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE170M;
        amf_color_profile = colorspace.full_range ? AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_601 : AMF_VIDEO_CONVERTER_COLOR_PROFILE_601;
        break;
      case video::colorspace_e::rec709:
        amf_primaries = AMF_COLOR_PRIMARIES_BT709;
        amf_transfer = AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709;
        amf_color_profile = colorspace.full_range ? AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_709 : AMF_VIDEO_CONVERTER_COLOR_PROFILE_709;
        break;
      case video::colorspace_e::bt2020sdr:
        amf_primaries = AMF_COLOR_PRIMARIES_BT2020;
        amf_transfer = AMF_COLOR_TRANSFER_CHARACTERISTIC_BT2020_10;
        amf_color_profile = colorspace.full_range ? AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_2020 : AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020;
        break;
      case video::colorspace_e::bt2020:
        amf_primaries = AMF_COLOR_PRIMARIES_BT2020;
        amf_transfer = AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084;
        amf_color_profile = colorspace.full_range ? AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_2020 : AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020;
        break;
      default:
        amf_primaries = AMF_COLOR_PRIMARIES_BT709;
        amf_transfer = AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709;
        amf_color_profile = colorspace.full_range ? AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_709 : AMF_VIDEO_CONVERTER_COLOR_PROFILE_709;
        break;
    }

    auto amf_bit_depth = (amf_int64)((colorspace.bit_depth == 10) ? AMF_COLOR_BIT_DEPTH_10 : AMF_COLOR_BIT_DEPTH_8);

    if (video_format == 0) {
      if (!set_required(AMF_VIDEO_ENCODER_COLOR_BIT_DEPTH, amf_bit_depth, "H.264 color bit depth")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_OUTPUT_COLOR_PROFILE, amf_color_profile, "H.264 output color profile")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_OUTPUT_TRANSFER_CHARACTERISTIC, amf_transfer, "H.264 output transfer characteristic")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_OUTPUT_COLOR_PRIMARIES, amf_primaries, "H.264 output color primaries")) return false;
    }
    else if (video_format == 1) {
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH, amf_bit_depth, "HEVC color bit depth")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_OUTPUT_COLOR_PROFILE, amf_color_profile, "HEVC output color profile")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_OUTPUT_TRANSFER_CHARACTERISTIC, amf_transfer, "HEVC output transfer characteristic")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_HEVC_OUTPUT_COLOR_PRIMARIES, amf_primaries, "HEVC output color primaries")) return false;
    }
    else {
      if (!set_required(AMF_VIDEO_ENCODER_AV1_COLOR_BIT_DEPTH, amf_bit_depth, "AV1 color bit depth")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_AV1_OUTPUT_COLOR_PROFILE, amf_color_profile, "AV1 output color profile")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_AV1_OUTPUT_TRANSFER_CHARACTERISTIC, amf_transfer, "AV1 output transfer characteristic")) return false;
      if (!set_required(AMF_VIDEO_ENCODER_AV1_OUTPUT_COLOR_PRIMARIES, amf_primaries, "AV1 output color primaries")) return false;
    }

    // Save statistics feedback state for encode_frame()
    statistics_enabled = config.enable_statistics_feedback;
    psnr_enabled = config.enable_psnr_feedback;
    ssim_enabled = config.enable_ssim_feedback;

    // Pre-Analysis sub-system properties (set on encoder when PA is enabled)
    if (preanalysis_plan.enabled) {
      if (config.pa_paq_mode && !set_verified_int64(AMF_PA_PAQ_MODE, *config.pa_paq_mode, "PAQ mode")) return false;
      if (config.pa_taq_mode && !set_verified_int64(AMF_PA_TAQ_MODE, *config.pa_taq_mode, "TAQ mode")) return false;
      if (config.pa_caq_strength && !set_verified_int64(AMF_PA_CAQ_STRENGTH, *config.pa_caq_strength, "CAQ strength")) return false;
      if (config.pa_scene_change_sensitivity &&
          !set_verified_int64(
            AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY,
            *config.pa_scene_change_sensitivity,
            "PA scene-change sensitivity")) return false;
      if (config.pa_high_motion_quality_boost &&
          !set_verified_int64(
            AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE,
            *config.pa_high_motion_quality_boost,
            "PA high-motion quality boost")) return false;
      if (config.pa_initial_qp_after_scene_change &&
          !set_verified_int64(
            AMF_PA_INITIAL_QP_AFTER_SCENE_CHANGE,
            *config.pa_initial_qp_after_scene_change,
            "PA initial scene-change QP")) return false;
      if (config.pa_activity_type && !set_verified_int64(AMF_PA_ACTIVITY_TYPE, *config.pa_activity_type, "PA activity type")) return false;
    }

    // NOTE: LOWLATENCY_MODE is intentionally NOT forced here.
    //
    // Previously this block hard-coded AMF_VIDEO_ENCODER_(HEVC_)LOWLATENCY_MODE = true
    // for both H264 and HEVC (AV1 was never forced). This:
    //   1) Silently overrode the per-codec `config.lowlatency_mode` opt-in
    //      we set earlier in configure_*_encoder().
    //   2) Diverged from FFmpeg amfenc behavior, which only writes this
    //      property when the user passes `-latency 1` (default -1 = leave
    //      property unset, driver default = false).
    //   3) Triggered a firmware freeze on AMD RDNA4 (RX 9070/9070 XT) with
    //      Adrenalin 26.5.x on HEVC: video stalls while audio keeps flowing,
    //      toggling HDR (which forces encoder reinit) temporarily recovers.
    //      AV1 was unaffected precisely because no AV1 branch existed here.
    //
    // LOWLATENCY_MODE is now controlled solely by `config.lowlatency_mode`
    // (WebUI: amd_lowlatency_mode). Combined with USAGE = ULTRA_LOW_LATENCY,
    // the encoder pipeline still achieves low latency without the firmware
    // bug path. Users who want the aggressive mode can opt in explicitly.

    return true;
  }

  bool
  amf_d3d11::create_encoder(const amf_config &config,
    const video::config_t &client_config,
    const video::sunshine_colorspace_t &colorspace,
    platf::pix_fmt_e buffer_format) {
    // Determine video format from client config
    video_format = client_config.videoFormat;
    current_config = client_config;

    // Initialize AMF library
    if (!init_amf_library()) return false;

    // Create AMF context
    auto res = factory->CreateContext(&context);
    if (res != AMF_OK || !context) {
      BOOST_LOG(error) << "AMF: CreateContext failed, error: " << res;
      return false;
    }

    // Set surface cache size to match FFmpeg's hwcontext_amf initialization
    context->SetProperty(L"DeviceSurfaceCacheSize", (amf_int64) 50);

    // Initialize D3D11 in AMF context with DX11_1 (matching FFmpeg)
    res = context->InitDX11(device, AMF_DX11_1);
    if (res != AMF_OK) {
      BOOST_LOG(error) << "AMF: InitDX11 failed, error: " << res;
      return false;
    }

    // Create encoder component
    res = factory->CreateComponent(context, get_codec_id(), &encoder);
    if (res != AMF_OK || !encoder) {
      BOOST_LOG(error) << "AMF: CreateComponent failed for codec " << video_format << ", error: " << res;
      return false;
    }

    // Configure encoder properties (before Init)
    if (!configure_encoder(config, client_config, colorspace)) {
      return false;
    }

    // Initialize encoder
    auto amf_format = get_amf_format(buffer_format, colorspace.bit_depth);
    surface_format = amf_format;
    encode_width = client_config.width;
    encode_height = client_config.height;
    res = encoder->Init(amf_format, client_config.width, client_config.height);

    if (res != AMF_OK) {
      BOOST_LOG(error) << "AMF: encoder Init failed with the requested encode settings, error: " << res;
      return false;
    }

    // Some runtimes accept a property before Init but substitute a different
    // value while constructing the hardware pipeline. Verify the semantic pair
    // again after Init so probing cannot advertise a mode the driver did not keep.
    const auto applied_preanalysis_plan = lifecycle::resolve_preanalysis(config.rc_mode, config.preanalysis);
    if (config.preanalysis || applied_preanalysis_plan.enabled) {
      const wchar_t *preanalysis_property = video_format == 0 ? AMF_VIDEO_ENCODER_PRE_ANALYSIS_ENABLE :
                                            video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_PRE_ANALYSIS_ENABLE :
                                                                AMF_VIDEO_ENCODER_AV1_PRE_ANALYSIS_ENABLE;
      amf_bool applied_preanalysis = false;
      const auto preanalysis_result = encoder->GetProperty(preanalysis_property, &applied_preanalysis);
      if (preanalysis_result != AMF_OK || static_cast<bool>(applied_preanalysis) != applied_preanalysis_plan.enabled) {
        BOOST_LOG(error) << "AMF: driver changed the requested PreAnalysis state after Init"
                         << " (requested=" << applied_preanalysis_plan.enabled
                         << ", applied=" << static_cast<bool>(applied_preanalysis)
                         << ", result=" << preanalysis_result << ')';
        return false;
      }
      if (applied_preanalysis_plan.enabled) {
        amf_int64 applied_depth = 0;
        const auto depth_result = encoder->GetProperty(AMF_PA_LOOKAHEAD_BUFFER_DEPTH, &applied_depth);
        if (depth_result != AMF_OK || applied_depth != preanalysis_lookahead_depth) {
          BOOST_LOG(error) << "AMF: driver changed the requested PreAnalysis depth after Init"
                           << " (requested=" << preanalysis_lookahead_depth << ", applied=" << applied_depth
                           << ", result=" << depth_result << ')';
          return false;
        }
      }
    }

    if (config.rc_mode) {
      const wchar_t *rate_control_property = video_format == 0 ? AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD :
                                             video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD :
                                                                 AMF_VIDEO_ENCODER_AV1_RATE_CONTROL_METHOD;
      amf_int64 applied_rate_control = -1;
      const auto rate_control_result = encoder->GetProperty(rate_control_property, &applied_rate_control);
      if (rate_control_result != AMF_OK || applied_rate_control != *config.rc_mode) {
        BOOST_LOG(error) << "AMF: driver changed the requested rate-control mode after Init"
                         << " (requested=" << *config.rc_mode << ", applied=" << applied_rate_control
                         << ", result=" << rate_control_result << ')';
        return false;
      }
    }

    const bool adaptive_quantization_supported_after_init =
      lifecycle::rate_control_supports_adaptive_quantization(config.rc_mode);
    if (!adaptive_quantization_supported_after_init) {
      if (video_format == 2) {
        amf_int64 applied_aq_mode = AMF_VIDEO_ENCODER_AV1_AQ_MODE_CAQ;
        const auto aq_result = encoder->GetProperty(AMF_VIDEO_ENCODER_AV1_AQ_MODE, &applied_aq_mode);
        if (aq_result != AMF_OK || applied_aq_mode != AMF_VIDEO_ENCODER_AV1_AQ_MODE_NONE) {
          BOOST_LOG(error) << "AMF: driver enabled AV1 adaptive quantization with CQP after Init"
                           << " (applied=" << applied_aq_mode << ", result=" << aq_result << ')';
          return false;
        }
      } else {
        const wchar_t *vbaq_property = video_format == 0 ? AMF_VIDEO_ENCODER_ENABLE_VBAQ :
                                                          AMF_VIDEO_ENCODER_HEVC_ENABLE_VBAQ;
        amf_bool applied_vbaq = true;
        const auto vbaq_result = encoder->GetProperty(vbaq_property, &applied_vbaq);
        if (vbaq_result != AMF_OK || static_cast<bool>(applied_vbaq)) {
          BOOST_LOG(error) << "AMF: driver enabled VBAQ with CQP after Init"
                           << " (applied=" << static_cast<bool>(applied_vbaq)
                           << ", result=" << vbaq_result << ')';
          return false;
        }
      }
    }

    {
      const wchar_t *skip_frame_property = video_format == 0 ? AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE :
                                             video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_SKIP_FRAME_ENABLE :
                                                                 AMF_VIDEO_ENCODER_AV1_RATE_CONTROL_SKIP_FRAME;
      amf_bool applied_skip_frame = true;
      const auto skip_frame_result = encoder->GetProperty(skip_frame_property, &applied_skip_frame);
      if (skip_frame_result != AMF_OK || static_cast<bool>(applied_skip_frame)) {
        BOOST_LOG(error) << "AMF: driver changed the requested rate-control frame-skipping state after Init"
                         << " (requested=false, applied=" << static_cast<bool>(applied_skip_frame)
                         << ", result=" << skip_frame_result << ')';
        return false;
      }
    }

    if (config.qvbr_quality_level && config.rc_mode && *config.rc_mode == 4) {
      const wchar_t *qvbr_quality_property = video_format == 0 ? AMF_VIDEO_ENCODER_QVBR_QUALITY_LEVEL :
                                               video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_QVBR_QUALITY_LEVEL :
                                                                   AMF_VIDEO_ENCODER_AV1_QVBR_QUALITY_LEVEL;
      amf_int64 applied_qvbr_quality = 0;
      const auto qvbr_result = encoder->GetProperty(qvbr_quality_property, &applied_qvbr_quality);
      if (qvbr_result != AMF_OK || applied_qvbr_quality != *config.qvbr_quality_level) {
        BOOST_LOG(error) << "AMF: driver changed the requested QVBR quality after Init"
                         << " (requested=" << *config.qvbr_quality_level << ", applied=" << applied_qvbr_quality
                         << ", result=" << qvbr_result << ')';
        return false;
      }
    }

    // INPUT_QUEUE_SIZE is a static encoder property. Read back the value after
    // Init because it determines how many external textures AMF may own at once.
    // An explicit setting must survive Init; for the driver default, use the
    // applied value to size the lazy direct-render pool accurately.
    {
      const wchar_t *input_queue_property = video_format == 0 ? AMF_VIDEO_ENCODER_INPUT_QUEUE_SIZE :
                                              video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_INPUT_QUEUE_SIZE :
                                                                  AMF_VIDEO_ENCODER_AV1_INPUT_QUEUE_SIZE;
      amf_int64 applied_input_queue_size = 0;
      const auto queue_result = encoder->GetProperty(input_queue_property, &applied_input_queue_size);
      const bool valid_queue_size = queue_result == AMF_OK &&
                                    applied_input_queue_size >= 1 &&
                                    applied_input_queue_size <= static_cast<amf_int64>(lifecycle::maximum_amf_input_queue_size);
      if (config.input_queue_size &&
          (!valid_queue_size || applied_input_queue_size != *config.input_queue_size)) {
        BOOST_LOG(error) << "AMF: driver changed the requested input queue size after Init"
                         << " (requested=" << *config.input_queue_size
                         << ", applied=" << applied_input_queue_size
                         << ", result=" << queue_result << ')';
        return false;
      }
      if (valid_queue_size) {
        encoder_input_queue_size = static_cast<std::size_t>(applied_input_queue_size);
      } else {
        encoder_input_queue_size = lifecycle::default_amf_input_queue_size;
        BOOST_LOG(warning) << "AMF: could not read the applied input queue size; reserving for the documented default of "
                           << encoder_input_queue_size;
      }
    }

    // Derive runtime watchdog threshold from framerate so the fatal-error
    // signal fires after roughly 1s of wall-clock time regardless of fps.
    // Floor at 30 so brief driver scheduling stalls do not cause false reinit.
    {
      int fps = client_config.framerate > 0 ? client_config.framerate : 60;
      max_consecutive_failures = std::max(30, fps);
    }

    // Check if driver supports QUERY_TIMEOUT by reading back the property (FFmpeg pattern)
    {
      const wchar_t *qt_prop = (video_format == 0) ? AMF_VIDEO_ENCODER_QUERY_TIMEOUT :
                               (video_format == 1) ? AMF_VIDEO_ENCODER_HEVC_QUERY_TIMEOUT :
                                                     AMF_VIDEO_ENCODER_AV1_QUERY_TIMEOUT;
      amf_int64 qt_val = 0;
      auto qt_res = encoder->GetProperty(qt_prop, &qt_val);
      query_timeout_supported = qt_res == AMF_OK && qt_val > 0;
      BOOST_LOG(info) << "AMF: QUERY_TIMEOUT " << (query_timeout_supported ? "supported" : "not supported") << " (value=" << qt_val << ")";
    }

    if (video_format == 2) {
      amf_int64 applied_latency_mode = -1;
      const auto latency_result = encoder->GetProperty(
        AMF_VIDEO_ENCODER_AV1_ENCODING_LATENCY_MODE,
        &applied_latency_mode);
      BOOST_LOG(debug) << "AMF: applied AV1 encoding latency mode=" << applied_latency_mode
                       << " (result=" << latency_result << ')';
    }

    // Create the rotating textures that are both render targets and native AMF
    // inputs. Rendering directly into a reserved slot removes a full-frame GPU copy.
    DXGI_FORMAT dxgi_fmt;
    switch (buffer_format) {
      case platf::pix_fmt_e::nv12:
        dxgi_fmt = DXGI_FORMAT_NV12;
        break;
      case platf::pix_fmt_e::p010:
        dxgi_fmt = DXGI_FORMAT_P010;
        break;
      default:
        dxgi_fmt = (colorspace.bit_depth == 10) ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
        break;
    }

    input_surface_desc = {};
    input_surface_desc.Width = client_config.width;
    input_surface_desc.Height = client_config.height;
    input_surface_desc.MipLevels = 1;
    input_surface_desc.ArraySize = 1;
    input_surface_desc.Format = dxgi_fmt;
    input_surface_desc.SampleDesc.Count = 1;
    input_surface_desc.Usage = D3D11_USAGE_DEFAULT;
    // PreAnalysis reads the converted input through its analysis shaders.  A
    // render-target-only texture can be accepted by ordinary AMF modes but may
    // stall or permanently backpressure QVBR/HQVBR/HQCBR on current Radeon
    // drivers because no shader-resource view can be created for the input.
    input_surface_desc.BindFlags = D3D11_BIND_RENDER_TARGET |
                                   (preanalysis_enabled ? D3D11_BIND_SHADER_RESOURCE : 0);

    for (auto &slot : input_surface_ring) {
      slot.texture.Reset();
      slot.frame_index = 0;
      slot.state = input_surface_state_e::free;
      slot.release_notified = false;
    }
    next_input_surface_slot = 0;
    active_input_surface_count = lifecycle::input_surface_count_for_pipeline(
      static_cast<int>(encoder_input_queue_size),
      preanalysis_lookahead_depth);
    if (!ensure_input_surface_count(active_input_surface_count)) {
      return false;
    }
    prepared_input_surface_slot.reset();
    last_rendered_input_surface_slot.reset();



    // Clamp effective LTR slots to what the encoder actually reserves.
    // When max_ltr_frames == 0 (default), the entire LTR/RFI subsystem becomes
    // a no-op: the IDR baseline marking, slot rotation, and invalidate handling
    // below all gate on `effective_ltr_slots > 0`. The fallback for client-side
    // invalidate_ref_frames in this case is force_idr=true (see video.cpp).
    effective_ltr_slots = (max_ltr_frames > 0) ? std::min(max_ltr_frames, MAX_LTR_SLOTS) : 0;

    // Reset LTR state
    for (auto &valid : ltr_slots_valid) valid = false;
    for (auto &fi : ltr_slot_frame_index) fi = 0;
    current_ltr_slot = 0;
    rfi_pending = false;
    input_surfaces_in_flight = 0;
    accepted_input_count = 0;
    completed_output_count = 0;
    consecutive_submit_failures = 0;
    consecutive_surface_failures = 0;
    consecutive_query_failures = 0;
    consecutive_output_failures = 0;
    consecutive_catchup_misses = 0;
    last_output_progress = std::chrono::steady_clock::now();
    submit_backpressure_started = {};
    completed_outputs.clear();
    frame_rfi_flags.clear();
    last_completed_frame_index = 0;
    last_submitted_frame_index = 0;
    output_fatal = false;
    drain_requested = false;
    drain_complete = false;
    output_poll_requested = false;
    active_output_poll_waiters = 0;
    catchup_batch_count = 0;

    output_thread = std::jthread([this](std::stop_token stop_token) {
      output_pump(stop_token);
    });

    auto codec_name = (video_format == 0) ? "H.264" :
                      (video_format == 1) ? "HEVC" :
                      (video_format == 2) ? "AV1" : "Unknown";
    BOOST_LOG(info) << "AMF: standalone " << codec_name << " encoder created ("
                    << client_config.width << "x" << client_config.height << " @ "
                    << client_config.framerate << "fps, LTR=" << max_ltr_frames
                    << ", PA=" << (preanalysis_enabled ? "on" : "off")
                    << ", lookahead=" << preanalysis_lookahead_depth
                    << ", input_queue=" << encoder_input_queue_size
                    << ", input_surfaces=" << active_input_surface_count
                    << ", slices=" << client_config.slicesPerFrame << ")";
    return true;
  }

  void
  amf_d3d11::destroy_encoder() {
    if (output_thread.joinable()) {
      output_thread.request_stop();
      state_cv.notify_all();
      output_thread.join();
    }
    if (encoder) {
      encoder->Terminate();
      encoder = nullptr;
    }
    if (context) {
      context->Terminate();
      context = nullptr;
    }
    for (auto &slot : input_surface_ring) {
      slot.texture.Reset();
      slot.frame_index = 0;
      slot.state = input_surface_state_e::free;
      slot.release_notified = false;
    }
    {
      std::lock_guard lock(state_mutex);
      prepared_input_surface_slot.reset();
      last_rendered_input_surface_slot.reset();
      completed_outputs.clear();
      frame_rfi_flags.clear();
      input_surfaces_in_flight = 0;
      accepted_input_count = 0;
      completed_output_count = 0;
      last_completed_frame_index = 0;
      last_submitted_frame_index = 0;
      output_fatal = false;
      drain_requested = false;
      drain_complete = false;
      output_poll_requested = false;
      active_output_poll_waiters = 0;
      catchup_batch_count = 0;
      consecutive_output_failures = 0;
    }
    last_output_progress = {};
    submit_backpressure_started = {};
    preanalysis_enabled = false;
    preanalysis_lookahead_depth = 0;
    query_timeout_supported = false;
    encoder_input_queue_size = lifecycle::default_amf_input_queue_size;
    input_surface_desc = {};

    if (amf_dll) {
      FreeLibrary(amf_dll);
      amf_dll = nullptr;
    }
    factory = nullptr;
  }

  amf_encoded_frame
  amf_d3d11::extract_encoded_frame(const ::amf::AMFDataPtr &output_data) {
    amf_encoded_frame result;
    if (!output_data) {
      return result;
    }

    const auto output_pts = output_data->GetPts();
    if (output_pts >= 0) {
      result.frame_index = static_cast<uint64_t>(output_pts);
    }
    ::amf::AMFBufferPtr buffer(output_data);
    if (!buffer) {
      BOOST_LOG(error) << "AMF: output is not a buffer";
      result.fatal = true;
      return result;
    }

    auto data_ptr = static_cast<uint8_t *>(buffer->GetNative());
    auto data_size = buffer->GetSize();
    if (!data_ptr || data_size == 0) {
      BOOST_LOG(error) << "AMF: encoder returned an empty output buffer for frame " << result.frame_index;
      return result;
    }
    result.data.assign(data_ptr, data_ptr + data_size);

    // after_ref_frame_invalidation is resolved by the pump under state_mutex.
    // This function deliberately touches no lock-protected members so the
    // bitstream copy and driver property reads can run outside the lock.

    amf_int64 output_type = 0;
    if (video_format == 0) {
      if (output_data->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &output_type) == AMF_OK) {
        result.idr = (output_type == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR);
      }
    }
    else if (video_format == 1) {
      if (output_data->GetProperty(AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE, &output_type) == AMF_OK) {
        result.idr = (output_type == AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_IDR);
      }
    }
    else {
      if (output_data->GetProperty(AMF_VIDEO_ENCODER_AV1_OUTPUT_FRAME_TYPE, &output_type) == AMF_OK) {
        result.idr = (output_type == AMF_VIDEO_ENCODER_AV1_OUTPUT_FRAME_TYPE_KEY);
      }
    }

    if (statistics_enabled) {
      amf_int64 avg_qp = 0;
      const wchar_t *avg_qp_prop = (video_format == 0) ? AMF_VIDEO_ENCODER_STATISTIC_AVERAGE_QP :
                                   (video_format == 1) ? AMF_VIDEO_ENCODER_HEVC_STATISTIC_AVERAGE_QP :
                                                         AMF_VIDEO_ENCODER_AV1_STATISTIC_AVERAGE_Q_INDEX;
      if (output_data->GetProperty(avg_qp_prop, &avg_qp) == AMF_OK) {
        BOOST_LOG(debug) << "AMF: frame " << result.frame_index << " avg_qp=" << avg_qp << " size=" << data_size;
      }
    }
    if (psnr_enabled) {
      double psnr_y = 0;
      const wchar_t *psnr_prop = (video_format == 0) ? AMF_VIDEO_ENCODER_STATISTIC_PSNR_Y :
                                 (video_format == 1) ? AMF_VIDEO_ENCODER_HEVC_STATISTIC_PSNR_Y :
                                                       AMF_VIDEO_ENCODER_AV1_STATISTIC_PSNR_Y;
      if (output_data->GetProperty(psnr_prop, &psnr_y) == AMF_OK) {
        BOOST_LOG(debug) << "AMF: frame " << result.frame_index << " PSNR_Y=" << psnr_y;
      }
    }
    if (ssim_enabled) {
      double ssim_y = 0;
      const wchar_t *ssim_prop = (video_format == 0) ? AMF_VIDEO_ENCODER_STATISTIC_SSIM_Y :
                                 (video_format == 1) ? AMF_VIDEO_ENCODER_HEVC_STATISTIC_SSIM_Y :
                                                       AMF_VIDEO_ENCODER_AV1_STATISTIC_SSIM_Y;
      if (output_data->GetProperty(ssim_prop, &ssim_y) == AMF_OK) {
        BOOST_LOG(debug) << "AMF: frame " << result.frame_index << " SSIM_Y=" << ssim_y;
      }
    }

    return result;
  }

  void
  amf_d3d11::output_pump(std::stop_token stop_token) noexcept {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    try {
      while (!stop_token.stop_requested()) {
        {
          std::unique_lock lock(state_mutex);
          state_cv.wait(lock, [&]() {
            return stop_token.stop_requested() || output_poll_requested || drain_requested || output_fatal;
          });
          if (stop_token.stop_requested() || output_fatal) {
            break;
          }
        }

        uint64_t queried_through_input = 0;
        {
          std::lock_guard lock(state_mutex);
          queried_through_input = accepted_input_count;
        }

        ::amf::AMFDataPtr output_data;
        const auto query_result = encoder ? encoder->QueryOutput(&output_data) : AMF_FAIL;
        if (stop_token.stop_requested()) {
          break;
        }

        if (output_data) {
          // Extract outside state_mutex: the bitstream assign() is multi-MB on
          // high-bitrate IDR frames and encode_frame takes this same mutex as
          // its first action — copying under the lock adds submit-path jitter.
          auto encoded_frame = extract_encoded_frame(output_data);
          {
            std::lock_guard lock(state_mutex);
            if (encoded_frame.data.empty()) {
              if (++consecutive_output_failures >= max_consecutive_failures) {
                BOOST_LOG(error) << "AMF: encoder repeatedly returned invalid output; signaling reinit";
                output_fatal = true;
              }
            } else {
              auto rfi_flag = frame_rfi_flags.find(encoded_frame.frame_index);
              if (rfi_flag != frame_rfi_flags.end()) {
                encoded_frame.after_ref_frame_invalidation = rfi_flag->second;
                frame_rfi_flags.erase(rfi_flag);
              }
              while (frame_rfi_flags.size() > 256) {
                frame_rfi_flags.erase(frame_rfi_flags.begin());
              }
              consecutive_output_failures = 0;
              ++completed_output_count;
              last_completed_frame_index = std::max(last_completed_frame_index, encoded_frame.frame_index);
              last_output_progress = std::chrono::steady_clock::now();
              consecutive_query_failures = 0;
              completed_outputs.emplace_back(std::move(encoded_frame));
            }
          }
          state_cv.notify_all();
          continue;
        }

        if (query_result == AMF_EOF) {
          bool expected_eof = false;
          {
            std::lock_guard lock(state_mutex);
            expected_eof = drain_requested;
            if (!expected_eof) {
              BOOST_LOG(error) << "AMF: output pump reached EOF without a drain request";
              output_fatal = true;
            }
            drain_complete = expected_eof && !output_fatal;
          }
          state_cv.notify_all();
          break;
        }

        const bool no_output_available = query_result == AMF_OK ||
                                         query_result == AMF_REPEAT ||
                                         query_result == AMF_NEED_MORE_INPUT;
        if (no_output_available) {
          bool poll_disarmed = false;
          {
            std::lock_guard lock(state_mutex);
            consecutive_query_failures = 0;
            if (lifecycle::should_disarm_output_poll(
                  queried_through_input,
                  accepted_input_count,
                  drain_requested,
                  active_output_poll_waiters)) {
              // No bounded waiter remains for this accepted generation. Sleep
              // until a new input explicitly re-arms polling; an output that
              // legitimately never arrives must not leave a permanent poll loop.
              // Do not clear a re-arm from SubmitInput that raced this query.
              output_poll_requested = false;
              poll_disarmed = true;
            }
          }
          if (!poll_disarmed) {
            // Drain, an active waiter, or a concurrent submission keeps polling
            // armed. QUERY_TIMEOUT-backed OK/REPEAT calls already blocked for up
            // to 1 ms. Some runtimes return NEED_MORE_INPUT immediately even from
            // QueryOutput, so pace that defensive compatibility case rather
            // than hot-spinning an above-normal-priority thread — but stay
            // interruptible so stop/drain/new-input wake the pump immediately
            // instead of eating the remainder of a fixed sleep quantum.
            if (query_result == AMF_NEED_MORE_INPUT || !query_timeout_supported) {
              std::unique_lock lock(state_mutex);
              state_cv.wait_for(lock, std::chrono::milliseconds(1), [&]() {
                return stop_token.stop_requested() || output_fatal || drain_requested;
              });
            }
          }
          continue;
        }

        {
          bool fatal = false;
          {
            std::lock_guard lock(state_mutex);
            const auto removed_reason = device ? device->GetDeviceRemovedReason() : S_OK;
            if (removed_reason != S_OK) {
              BOOST_LOG(error) << "AMF: output pump detected D3D11 device loss, reason: 0x"
                               << util::hex(removed_reason).to_string_view();
              output_fatal = true;
              fatal = true;
            } else if (++consecutive_query_failures >= max_consecutive_failures && !output_fatal) {
              BOOST_LOG(error) << "AMF: output pump observed repeated QueryOutput failures (error="
                               << query_result << "); signaling reinit";
              output_fatal = true;
              fatal = true;
            }
          }
          if (fatal) {
            state_cv.notify_all();
            break;
          }
          // Interruptible failure backoff — stop requests must not wait out
          // the sleep quantum.
          std::unique_lock lock(state_mutex);
          state_cv.wait_for(lock, std::chrono::milliseconds(1), [&]() {
            return stop_token.stop_requested() || output_fatal;
          });
        }
      }
    } catch (const std::exception &ex) {
      BOOST_LOG(error) << "AMF: output pump failed: " << ex.what();
      std::lock_guard lock(state_mutex);
      output_fatal = true;
      state_cv.notify_all();
    } catch (...) {
      BOOST_LOG(error) << "AMF: output pump failed with an unknown exception";
      std::lock_guard lock(state_mutex);
      output_fatal = true;
      state_cv.notify_all();
    }
  }

  amf_encode_result
  amf_d3d11::encode_frame(uint64_t frame_index, bool force_idr) {
    amf_encode_result result;
    auto &results = result.frames;

    if (!encoder) {
      result.fatal = true;
      return result;
    }

    std::size_t slot_index = 0;
    std::optional<std::size_t> duplicate_source_slot;
    uint64_t completed_before_submission = 0;
    {
      std::unique_lock lock(state_mutex);
      auto drain_completed_outputs = [&]() {
        while (!completed_outputs.empty()) {
          results.emplace_back(std::move(completed_outputs.front()));
          completed_outputs.pop_front();
        }
      };
      drain_completed_outputs();
      completed_before_submission = completed_output_count;
      if (output_fatal) {
        result.fatal = true;
        return result;
      }

      if (prepared_input_surface_slot) {
        slot_index = *prepared_input_surface_slot;
        prepared_input_surface_slot.reset();
        last_rendered_input_surface_slot = slot_index;
      } else if (last_rendered_input_surface_slot) {
        // Sunshine intentionally encodes the most recent converted image again when
        // capture is static/times out (minimum-FPS refresh, IDR requests, and encoder
        // probing). Prefer the same direct-render surface once AMF releases it. A
        // PA lookahead may still own that slot, so rotate to another free texture
        // and copy the last converted image there rather than starving lookahead.
        const auto source_slot = *last_rendered_input_surface_slot;
        auto find_repeat_slot = [&]() -> std::optional<std::size_t> {
          return lifecycle::select_repeat_surface(
            input_surface_ring,
            source_slot,
            next_input_surface_slot,
            active_input_surface_count);
        };
        auto repeat_slot = find_repeat_slot();
        if (!repeat_slot && !output_fatal && active_input_surface_count < input_surface_ring.size()) {
          // Allocate outside the lock: a multi-MB CreateTexture2D under
          // state_mutex stalls the pump and AMF's release observers. Expansion
          // only ever runs on this thread, so re-checking after relock only has
          // to account for slots the observers freed meanwhile.
          lock.unlock();
          auto expansion_texture = create_input_surface_texture();
          lock.lock();
          repeat_slot = find_repeat_slot();
          if (!repeat_slot && expansion_texture && !output_fatal &&
              active_input_surface_count < input_surface_ring.size()) {
            auto &expansion_slot = input_surface_ring[active_input_surface_count];
            if (!expansion_slot.texture) {
              expansion_slot.texture = std::move(expansion_texture);
            }
            repeat_slot = active_input_surface_count;
            ++active_input_surface_count;
            BOOST_LOG(debug) << "AMF: expanded direct-render input pool to " << active_input_surface_count
                             << " surfaces for repeated input";
          }
        }
        if (!repeat_slot) {
          state_cv.wait_for(lock, std::chrono::milliseconds(20), [&]() {
            return output_fatal || find_repeat_slot().has_value();
          });
          repeat_slot = find_repeat_slot();
        }
        drain_completed_outputs();
        if (output_fatal) {
          result.fatal = true;
          return result;
        }
        if (!repeat_slot) {
          // The previous submission is still owned by AMF. Dropping this minimum-FPS
          // duplicate is safer than reusing an in-flight surface; the pump can still
          // return completed output in this result.
          return result;
        }
        slot_index = *repeat_slot;
        if (slot_index != source_slot) {
          duplicate_source_slot = source_slot;
        }
        input_surface_ring[slot_index].state = input_surface_state_e::reserved;
        input_surface_ring[slot_index].release_notified = false;
        last_rendered_input_surface_slot = slot_index;
        next_input_surface_slot = (slot_index + 1) % active_input_surface_count;
      } else {
        BOOST_LOG(error) << "AMF: encode called before any input surface was rendered";
        result.fatal = true;
        return result;
      }
    }

    auto &input_slot = input_surface_ring[slot_index];
    auto release_reserved_slot = util::fail_guard([&]() {
      std::lock_guard lock(state_mutex);
      if (input_slot.state == input_surface_state_e::reserved) {
        input_slot.state = input_surface_state_e::free;
        input_slot.frame_index = 0;
        state_cv.notify_all();
      }
    });
    if (duplicate_source_slot) {
      Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
      device->GetImmediateContext(immediate_context.GetAddressOf());
      if (!immediate_context) {
        BOOST_LOG(error) << "AMF: could not acquire D3D11 context for repeated lookahead input";
        result.fatal = true;
        return result;
      }
      immediate_context->CopyResource(
        input_slot.texture.Get(),
        input_surface_ring[*duplicate_source_slot].texture.Get());
    }
    if (const auto removed_reason = device->GetDeviceRemovedReason(); removed_reason != S_OK) {
      BOOST_LOG(error) << "AMF: D3D11 device lost before native input submission, reason: 0x"
                       << util::hex(removed_reason).to_string_view();
      result.fatal = true;
      return result;
    }

    // Each wrapper references a distinct ring texture. AMF may retain an input after
    // producing output (or drop it without output), so recycle the texture only from
    // AMFSurfaceObserver::OnSurfaceDataRelease -- never by guessing from output PTS.
    ::amf::AMFSurfacePtr surface;
    auto res = context->CreateSurfaceFromDX11Native(
      input_slot.texture.Get(),
      &surface,
      &input_surface_release_observers[slot_index]);
    if (res != AMF_OK || !surface) {
      BOOST_LOG(error) << "AMF: CreateSurfaceFromDX11Native failed, error: " << res;
      const auto removed_reason = device->GetDeviceRemovedReason();
      if (removed_reason != S_OK) {
        BOOST_LOG(error) << "AMF: D3D11 device lost, reason: 0x" << util::hex(removed_reason).to_string_view();
        result.fatal = true;
      } else if (++consecutive_surface_failures >= max_consecutive_failures) {
        BOOST_LOG(error) << "AMF: repeated native surface creation failures; signaling reinit";
        result.fatal = true;
      }
      return result;
    }
    consecutive_surface_failures = 0;
    // Set crop to actual frame dimensions (hw surfaces can be vertically aligned by 16)
    surface->SetCrop(0, 0, encode_width, encode_height);
    surface->SetPts(static_cast<amf_pts>(frame_index));

    // Snapshot the recovery plan under the state lock, then release it before any
    // AMF call. SubmitInput may synchronously invoke OnSurfaceDataRelease(), which
    // must be able to acquire state_mutex without re-entering a non-recursive lock.
    bool frame_after_ref_frame_invalidation = false;
    int ltr_slot_to_commit = -1;
    int next_ltr_slot_to_commit = 0;
    bool consume_rfi_on_accept = false;
    bool reset_ltr_cache_on_accept = false;
    int ltr_slot_to_preserve_on_accept = -1;
    int ltr_reference_slot = -1;
    int effective_ltr_slots_snapshot = 0;
    {
      std::lock_guard lock(state_mutex);
      effective_ltr_slots_snapshot = rfi_enabled ? effective_ltr_slots : 0;
      next_ltr_slot_to_commit = current_ltr_slot;
      if (force_idr) {
        reset_ltr_cache_on_accept = true;
        consume_rfi_on_accept = true;
        if (effective_ltr_slots_snapshot > 0) {
          ltr_slot_to_commit = 0;
          next_ltr_slot_to_commit = (effective_ltr_slots_snapshot > 1) ? 1 : 0;
        }
      } else if (rfi_pending && effective_ltr_slots_snapshot > 0) {
        ltr_reference_slot = static_cast<int>(last_rfi_ltr_index);
        consume_rfi_on_accept = true;
        reset_ltr_cache_on_accept = true;
        ltr_slot_to_preserve_on_accept = ltr_reference_slot;
        frame_after_ref_frame_invalidation = true;
      } else if (effective_ltr_slots_snapshot > 0 && (frame_index % LTR_MARK_INTERVAL) == 0) {
        ltr_slot_to_commit = current_ltr_slot;
        if (effective_ltr_slots_snapshot > 1) {
          next_ltr_slot_to_commit = current_ltr_slot + 1;
          if (next_ltr_slot_to_commit >= effective_ltr_slots_snapshot) {
            next_ltr_slot_to_commit = 1;
          }
        }
      }
    }

    auto disable_rfi_after_property_failure = [&](const char *property_label, AMF_RESULT property_result) {
      BOOST_LOG(warning) << "AMF: failed to apply " << property_label << " (error=" << property_result
                         << "); disabling RFI and falling back to IDR recovery";
      std::lock_guard lock(state_mutex);
      rfi_enabled = false;
      max_ltr_frames = 0;
      effective_ltr_slots = 0;
      rfi_pending = false;
      for (auto &valid : ltr_slots_valid) valid = false;
      for (auto &marked_frame : ltr_slot_frame_index) marked_frame = 0;
    };

    auto set_ltr_surface_property = [&](const wchar_t *property, amf_int64 value, const char *label) {
      const auto property_result = surface->SetProperty(property, value);
      if (property_result == AMF_OK) {
        return true;
      }
      disable_rfi_after_property_failure(label, property_result);
      return false;
    };

    auto set_forced_idr_properties = [&]() {
      auto check = [&](AMF_RESULT property_result, const char *label) {
        if (property_result == AMF_OK) return true;
        BOOST_LOG(error) << "AMF: failed to set " << label << " on recovery frame, error=" << property_result;
        return false;
      };
      if (video_format == 0) {
        return check(surface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR), "H.264 IDR type") &&
               check(surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_SPS, true), "H.264 SPS insertion") &&
               check(surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_PPS, true), "H.264 PPS insertion");
      }
      if (video_format == 1) {
        return check(surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_IDR), "HEVC IDR type") &&
               check(surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_INSERT_HEADER, true), "HEVC header insertion");
      }
      return check(surface->SetProperty(AMF_VIDEO_ENCODER_AV1_FORCE_FRAME_TYPE, AMF_VIDEO_ENCODER_AV1_FORCE_FRAME_TYPE_KEY), "AV1 keyframe type") &&
             check(surface->SetProperty(AMF_VIDEO_ENCODER_AV1_FORCE_INSERT_SEQUENCE_HEADER, true), "AV1 sequence header insertion");
    };

    if (force_idr) {
      if (!set_forced_idr_properties()) {
        result.fatal = true;
        return result;
      }

      // After IDR, mark LTR slot 0 for RFI baseline
      if (ltr_slot_to_commit >= 0) {
        const wchar_t *mark_property = video_format == 0 ? AMF_VIDEO_ENCODER_MARK_CURRENT_WITH_LTR_INDEX :
                                       video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_MARK_CURRENT_WITH_LTR_INDEX :
                                                           AMF_VIDEO_ENCODER_AV1_MARK_CURRENT_WITH_LTR_INDEX;
        if (!set_ltr_surface_property(mark_property, 0, "IDR LTR baseline mark")) {
          ltr_slot_to_commit = -1;
        }
      }
    }
    else if (ltr_reference_slot >= 0) {
      // After RFI: force reference to the saved LTR frame
      const auto ltr_bitfield = static_cast<amf_int64>(1LL << ltr_reference_slot);
      const wchar_t *reference_property = video_format == 0 ? AMF_VIDEO_ENCODER_FORCE_LTR_REFERENCE_BITFIELD :
                                            video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_FORCE_LTR_REFERENCE_BITFIELD :
                                                                AMF_VIDEO_ENCODER_AV1_FORCE_LTR_REFERENCE_BITFIELD;
      if (!set_ltr_surface_property(reference_property, ltr_bitfield, "forced LTR reference")) {
        // Do not claim RFI recovery if the driver rejected the reference. Make
        // this same submission an IDR so the caller's recovery is not suppressed.
        frame_after_ref_frame_invalidation = false;
        consume_rfi_on_accept = false;
        ltr_slot_to_preserve_on_accept = -1;
        reset_ltr_cache_on_accept = true;
        if (!set_forced_idr_properties()) {
          result.fatal = true;
          return result;
        }
      }
    }
    else if (ltr_slot_to_commit >= 0) {
      // Periodically mark current frame as LTR for future RFI use.
      // Rotate through slots 1..N-1 so the IDR baseline in slot 0 stays valid
      // even if every recent periodic anchor lands inside a loss burst. With a
      // single slot configured, fall back to overwriting slot 0.
      const wchar_t *mark_property = video_format == 0 ? AMF_VIDEO_ENCODER_MARK_CURRENT_WITH_LTR_INDEX :
                                     video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_MARK_CURRENT_WITH_LTR_INDEX :
                                                         AMF_VIDEO_ENCODER_AV1_MARK_CURRENT_WITH_LTR_INDEX;
      if (!set_ltr_surface_property(mark_property, static_cast<amf_int64>(ltr_slot_to_commit), "periodic LTR mark")) {
        ltr_slot_to_commit = -1;
      }
    }

    if (statistics_enabled) {
      surface->SetProperty(video_format == 0 ? AMF_VIDEO_ENCODER_STATISTICS_FEEDBACK :
                           video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_STATISTICS_FEEDBACK :
                                               AMF_VIDEO_ENCODER_AV1_STATISTICS_FEEDBACK, true);
    }
    if (psnr_enabled) {
      surface->SetProperty(video_format == 0 ? AMF_VIDEO_ENCODER_PSNR_FEEDBACK :
                           video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_PSNR_FEEDBACK :
                                               AMF_VIDEO_ENCODER_AV1_PSNR_FEEDBACK, true);
    }
    if (ssim_enabled) {
      surface->SetProperty(video_format == 0 ? AMF_VIDEO_ENCODER_SSIM_FEEDBACK :
                           video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_SSIM_FEEDBACK :
                                               AMF_VIDEO_ENCODER_AV1_SSIM_FEEDBACK, true);
    }

    // Submit input — retry with output draining if input queue is still full (like FFmpeg).
    //
    // AMF SubmitInput return values we explicitly handle:
    //   AMF_OK                          — submitted, count it.
    //   AMF_INPUT_FULL                  — NOT consumed; encoder queue full, drain output + retry.
    //   AMF_DECODER_NO_FREE_SURFACES    — NOT consumed; surface pool exhausted, semantically
    //                                     equivalent to INPUT_FULL on the input side.
    //   AMF_NEED_MORE_INPUT             — CONSUMED; output is merely deferred (PreAnalysis
    //                                     lookahead priming). Never resubmit the same surface:
    //                                     FFmpeg's amfenc.c retries AMF_INPUT_FULL only and
    //                                     releases the surface for every other result.
    //   anything else                   — real error.
    auto retryable_submit = [](AMF_RESULT value) {
      return value == AMF_INPUT_FULL || value == AMF_DECODER_NO_FREE_SURFACES;
    };
    res = lifecycle::submit_with_bounded_retry(
      [&]() {
        return encoder->SubmitInput(surface);
      },
      [&]() {
        std::unique_lock lock(state_mutex);
        // QueryOutput may have disarmed itself after AMF_NEED_MORE_INPUT. A full
        // input queue is the opposite condition: queued work must be polled to
        // make room before this exact surface can be retried.
        output_poll_requested = true;
        state_cv.notify_all();
        state_cv.wait_for(lock, std::chrono::milliseconds(1), [&]() {
          return output_fatal || !completed_outputs.empty();
        });
        return output_fatal;
      },
      retryable_submit,
      20);
    if (retryable_submit(res)) {
      int in_flight = 0;
      {
        std::lock_guard lock(state_mutex);
        if (output_fatal) {
          result.fatal = true;
          return result;
        }
        in_flight = static_cast<int>(input_surfaces_in_flight);
      }
      const char *reason = res == AMF_INPUT_FULL ? "AMF_INPUT_FULL" : "AMF_DECODER_NO_FREE_SURFACES";
      BOOST_LOG(warning) << "AMF: SubmitInput still " << reason
                         << " after retries, dropping frame " << frame_index
                         << " (in_flight=" << in_flight << ")";
      const auto now = std::chrono::steady_clock::now();
      const auto exhausted_submissions = ++consecutive_submit_failures;
      if (submit_backpressure_started.time_since_epoch().count() == 0) {
        submit_backpressure_started = now;
      }
      const auto backpressure_start_known = submit_backpressure_started.time_since_epoch().count() != 0;
      const auto backpressure_duration = backpressure_start_known ?
                                           now - submit_backpressure_started :
                                           std::chrono::steady_clock::duration::zero();
      if (lifecycle::submit_backpressure_requires_reinit(
            exhausted_submissions,
            max_consecutive_failures,
            backpressure_start_known,
            backpressure_duration)) {
        BOOST_LOG(error) << "AMF: submit backpressure made no bounded progress; signaling reinit"
                         << " (consecutive=" << exhausted_submissions
                         << ", in_flight=" << in_flight << ')';
        result.fatal = true;
      }
      return result;
    }
    if (res == AMF_NEED_MORE_INPUT) {
      // The encoder consumed the surface and only defers output (PreAnalysis
      // lookahead priming). Account for it exactly like AMF_OK — the pump
      // already tolerates output arriving later than its input.
      res = AMF_OK;
    }
    if (res != AMF_OK) {
      BOOST_LOG(error) << "AMF: SubmitInput failed, error: " << res;
      // Check if the D3D11 device is lost (TDR, driver crash, etc.)
      if (device) {
        auto removed_reason = device->GetDeviceRemovedReason();
        if (removed_reason != S_OK) {
          BOOST_LOG(error) << "AMF: D3D11 device lost after SubmitInput, reason: 0x" << util::hex(removed_reason).to_string_view();
          result.fatal = true;
          return result;
        }
      }
      if (++consecutive_submit_failures >= max_consecutive_failures) {
        BOOST_LOG(error) << "AMF: " << consecutive_submit_failures << " consecutive SubmitInput failures, signaling reinit";
        result.fatal = true;
      }
      return result;
    }
    consecutive_submit_failures = 0;
    submit_backpressure_started = {};
    result.input_accepted = true;

    // Drop our wrapper reference immediately after acceptance. AMF retains its
    // own reference for as long as it owns the native texture; keeping ours until
    // the end of the bounded output wait would delay the observer callback and
    // make an otherwise free texture look busy for another 20-50 ms.
    surface = nullptr;

    std::unique_lock state_lock(state_mutex);
    const bool synchronously_released = lifecycle::on_input_accepted(input_slot, frame_index);
    last_submitted_frame_index = std::max(last_submitted_frame_index, frame_index);
    ++accepted_input_count;
    if (!synchronously_released) {
      ++input_surfaces_in_flight;
    }
    output_poll_requested = true;
    if (synchronously_released) {
      state_cv.notify_all();
    }
    release_reserved_slot.disable();
    const int effective_lookahead_depth = preanalysis_enabled ? preanalysis_lookahead_depth : 0;
    const bool output_was_expected = lifecycle::delayed_output_is_expected(
      accepted_input_count - 1,
      effective_lookahead_depth);
    const bool output_is_expected = lifecycle::delayed_output_is_expected(
      accepted_input_count,
      effective_lookahead_depth);
    if (!output_was_expected && output_is_expected) {
      // Start the watchdog only after the PA lookahead is primed. At low minimum
      // FPS, measuring from the first buffered frame would falsely treat the
      // intentional lookahead interval as an encoder stall.
      last_output_progress = std::chrono::steady_clock::now();
    }
    state_cv.notify_all();

    lifecycle::commit_recovery_state(
      result.input_accepted,
      effective_ltr_slots,
      reset_ltr_cache_on_accept,
      ltr_slot_to_preserve_on_accept,
      ltr_slot_to_commit,
      next_ltr_slot_to_commit,
      consume_rfi_on_accept,
      frame_after_ref_frame_invalidation,
      frame_index,
      ltr_slots_valid,
      ltr_slot_frame_index,
      current_ltr_slot,
      rfi_pending,
      [&](uint64_t recovered_frame_index) {
        frame_rfi_flags.emplace(recovered_frame_index, true);
      });

    // AMD's sample and FFmpeg both submit while a separate thread polls output.
    // Keep that poller alive for this short coalescing window: an early
    // AMF_NEED_MORE_INPUT is only a point-in-time result and must not disarm the
    // pump underneath the waiter. With no PA lookahead, wait through any older
    // queued output until this submission completes; otherwise one initial miss
    // becomes a permanent one-frame backlog.
    const auto output_wait_budget = lifecycle::output_coalesce_budget(current_config.framerate);
    bool coalesce_target_reached = lifecycle::output_coalesce_target_reached(
      frame_index,
      effective_lookahead_depth,
      completed_before_submission,
      completed_output_count,
      last_completed_frame_index);
    if (output_is_expected && !coalesce_target_reached) {
      ++active_output_poll_waiters;
      output_poll_requested = true;
      state_cv.notify_all();
      state_cv.wait_for(state_lock, output_wait_budget, [&]() {
        return output_fatal || lifecycle::output_coalesce_target_reached(
                                 frame_index,
                                 effective_lookahead_depth,
                                 completed_before_submission,
                                 completed_output_count,
                                 last_completed_frame_index);
      });
      --active_output_poll_waiters;
      state_cv.notify_all();
      coalesce_target_reached = lifecycle::output_coalesce_target_reached(
        frame_index,
        effective_lookahead_depth,
        completed_before_submission,
        completed_output_count,
        last_completed_frame_index);
    }
    while (!completed_outputs.empty()) {
      results.emplace_back(std::move(completed_outputs.front()));
      completed_outputs.pop_front();
    }
    if (output_fatal) {
      result.fatal = true;
    }

    if (!output_is_expected || coalesce_target_reached) {
      consecutive_catchup_misses = 0;
    } else if (++consecutive_catchup_misses % max_consecutive_failures == 0) {
      BOOST_LOG(warning) << "AMF: encoder output has not caught up for " << consecutive_catchup_misses
                         << " submitted frames (owned_surfaces=" << input_surfaces_in_flight
                         << ", accepted=" << accepted_input_count
                         << ", outputs=" << completed_output_count << ')';
    }

    const auto now = std::chrono::steady_clock::now();
    const bool output_still_expected = lifecycle::delayed_output_is_expected(
      accepted_input_count,
      effective_lookahead_depth);
    if (output_still_expected && last_output_progress.time_since_epoch().count() != 0 &&
        now - last_output_progress >= std::chrono::seconds(2)) {
      BOOST_LOG(error) << "AMF: accepted input but produced no output for 2 seconds; signaling reinit";
      result.fatal = true;
    }

    if (results.size() > 1 && ++catchup_batch_count % static_cast<uint64_t>(max_consecutive_failures) == 0) {
      BOOST_LOG(debug) << "AMF: observed " << catchup_batch_count
                       << " multi-frame catch-up batches; latest drained " << results.size()
                       << " frames through frame " << results.back().frame_index;
    }

    return result;
  }

  amf_encode_result
  amf_d3d11::drain_output(std::chrono::milliseconds timeout) {
    amf_encode_result result;
    std::unique_lock lock(state_mutex);
    state_cv.wait_for(lock, timeout, [&]() {
      return output_fatal || drain_complete || !completed_outputs.empty();
    });
    while (!completed_outputs.empty()) {
      result.frames.emplace_back(std::move(completed_outputs.front()));
      completed_outputs.pop_front();
    }
    result.fatal = output_fatal;
    return result;
  }

  bool
  amf_d3d11::begin_drain() {
    if (!encoder) {
      return false;
    }

    {
      std::lock_guard lock(state_mutex);
      if (drain_requested) {
        return true;
      }
      drain_requested = true;
      drain_complete = false;
      output_poll_requested = true;
    }
    state_cv.notify_all();

    auto retryable_drain = [](AMF_RESULT value) {
      return value == AMF_INPUT_FULL || value == AMF_REPEAT;
    };
    const auto drain_result = lifecycle::submit_with_bounded_retry(
      [&]() {
        return encoder->Drain();
      },
      [&]() {
        std::unique_lock lock(state_mutex);
        state_cv.wait_for(lock, std::chrono::milliseconds(1));
        return output_fatal;
      },
      retryable_drain,
      100);

    if (drain_result == AMF_OK || drain_result == AMF_EOF) {
      return true;
    }

    {
      std::lock_guard lock(state_mutex);
      drain_requested = false;
      output_poll_requested = false;
    }
    state_cv.notify_all();
    BOOST_LOG(error) << "AMF: failed to begin delayed-output drain, error: " << drain_result;
    return false;
  }

  bool
  amf_d3d11::invalidate_ref_frames(uint64_t first_frame, uint64_t last_frame) {
    std::lock_guard lock(state_mutex);
    if (!encoder || !rfi_enabled || effective_ltr_slots <= 0) return false;

    // Find a valid LTR slot whose frame was marked BEFORE the invalidation range.
    // This ensures we reference a frame that predates the corrupted frames.
    int best_ltr = -1;
    uint64_t best_frame = 0;
    for (int i = 0; i < effective_ltr_slots; i++) {
      if (ltr_slots_valid[i] && ltr_slot_frame_index[i] < first_frame) {
        if (best_ltr < 0 || ltr_slot_frame_index[i] > best_frame) {
          best_ltr = i;
          best_frame = ltr_slot_frame_index[i];
        }
      }
    }

    if (best_ltr < 0) {
      BOOST_LOG(warning) << "AMF: RFI failed, no valid LTR frame before frame " << first_frame;
      return false;
    }

    // Loss feedback arrives after newer frames may already have been submitted.
    // Those frames can depend transitively on the loss, so invalidate anchors
    // through the latest submitted frame, matching the NVENC recovery model.
    const auto effective_last_frame = std::max(last_frame, last_submitted_frame_index);
    for (int i = 0; i < effective_ltr_slots; i++) {
      if (ltr_slots_valid[i] && ltr_slot_frame_index[i] >= first_frame && ltr_slot_frame_index[i] <= effective_last_frame) {
        ltr_slots_valid[i] = false;
      }
    }

    last_rfi_ltr_index = best_ltr;
    rfi_pending = true;

    BOOST_LOG(info) << "AMF: RFI pending, using LTR index " << best_ltr
                    << " (frame " << best_frame << ") for invalidated frames " << first_frame << "-" << effective_last_frame;
    return true;
  }

  bool
  amf_d3d11::set_bitrate(int bitrate_kbps) {
    if (!encoder || bitrate_kbps <= 0) return false;

    auto bitrate = static_cast<int64_t>(bitrate_kbps) * 1000;
    // Keep the VBV at ~1 frame of bits on dynamic bitrate changes too, so a raised
    // bitrate can't start producing FEC-overflowing frames (see configure_encoder).
    AVRational fps {current_config.framerate > 0 ? current_config.framerate : 60, 1};
    const int64_t vbv_buffer_size = fps.num > 0 ? (bitrate * fps.den / fps.num) : bitrate;
    bool success = true;
    auto set_runtime_property = [&](const wchar_t *name, amf_int64 value, const char *label) {
      const auto property_result = encoder->SetProperty(name, value);
      if (property_result != AMF_OK) {
        BOOST_LOG(warning) << "AMF: runtime " << label << " update failed, error: " << property_result;
        success = false;
      }
    };

    if (video_format == 0) {
      set_runtime_property(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitrate, "target bitrate");
      if (user_configured_rate_control) {
        set_runtime_property(AMF_VIDEO_ENCODER_PEAK_BITRATE, bitrate, "peak bitrate");
        set_runtime_property(AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, vbv_buffer_size, "VBV buffer");
      }
      if (enforce_hrd_enabled) set_runtime_property(AMF_VIDEO_ENCODER_MAX_AU_SIZE, vbv_buffer_size * 4, "maximum access-unit size");
    }
    else if (video_format == 1) {
      set_runtime_property(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitrate, "target bitrate");
      if (user_configured_rate_control) {
        set_runtime_property(AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE, bitrate, "peak bitrate");
        set_runtime_property(AMF_VIDEO_ENCODER_HEVC_VBV_BUFFER_SIZE, vbv_buffer_size, "VBV buffer");
      }
      if (enforce_hrd_enabled) set_runtime_property(AMF_VIDEO_ENCODER_HEVC_MAX_AU_SIZE, vbv_buffer_size * 4, "maximum access-unit size");
    }
    else {
      set_runtime_property(AMF_VIDEO_ENCODER_AV1_TARGET_BITRATE, bitrate, "target bitrate");
      if (user_configured_rate_control) {
        set_runtime_property(AMF_VIDEO_ENCODER_AV1_PEAK_BITRATE, bitrate, "peak bitrate");
        set_runtime_property(AMF_VIDEO_ENCODER_AV1_VBV_BUFFER_SIZE, vbv_buffer_size, "VBV buffer");
      }
      if (enforce_hrd_enabled) set_runtime_property(AMF_VIDEO_ENCODER_AV1_MAX_COMPRESSED_FRAME_SIZE, vbv_buffer_size * 4, "maximum compressed-frame size");
    }

    if (success) {
      current_config.bitrate = bitrate_kbps;
      BOOST_LOG(info) << "AMF: bitrate dynamically changed to " << bitrate_kbps << " Kbps";
    }
    else {
      BOOST_LOG(warning) << "AMF: live bitrate update was incomplete; encoder rebuild required";
    }
    return success;
  }

  bool
  amf_d3d11::set_hdr_metadata(const std::optional<amf_hdr_metadata> &metadata) {
    if (!encoder || !context || video_format < 0 || video_format > 2) return false;

    const wchar_t *property = video_format == 0 ? AMF_VIDEO_ENCODER_INPUT_HDR_METADATA :
                              video_format == 1 ? AMF_VIDEO_ENCODER_HEVC_INPUT_HDR_METADATA :
                                                  AMF_VIDEO_ENCODER_AV1_INPUT_HDR_METADATA;
    if (!metadata) {
      const ::amf::AMFBufferPtr empty_metadata;
      const auto property_result = encoder->SetProperty(property, empty_metadata);
      if (property_result != AMF_OK) {
        BOOST_LOG(warning) << "AMF: failed to clear HDR metadata, error: " << property_result;
        return false;
      }
      return true;
    }

    {
      // Create AMFBuffer containing AMFHDRMetadata
      ::amf::AMFBufferPtr hdr_buffer;
      auto res = context->AllocBuffer(::amf::AMF_MEMORY_HOST, sizeof(AMFHDRMetadata), &hdr_buffer);
      if (res != AMF_OK || !hdr_buffer) {
        BOOST_LOG(warning) << "AMF: failed to allocate HDR metadata buffer";
        return false;
      }

      auto *amf_hdr = static_cast<AMFHDRMetadata *>(hdr_buffer->GetNative());
      // Display primaries: both normalized to 50,000
      amf_hdr->redPrimary[0] = metadata->displayPrimaries[0].x;
      amf_hdr->redPrimary[1] = metadata->displayPrimaries[0].y;
      amf_hdr->greenPrimary[0] = metadata->displayPrimaries[1].x;
      amf_hdr->greenPrimary[1] = metadata->displayPrimaries[1].y;
      amf_hdr->bluePrimary[0] = metadata->displayPrimaries[2].x;
      amf_hdr->bluePrimary[1] = metadata->displayPrimaries[2].y;
      amf_hdr->whitePoint[0] = metadata->whitePoint.x;
      amf_hdr->whitePoint[1] = metadata->whitePoint.y;
      // maxMasteringLuminance: AMF expects nits * 10000, SS_HDR_METADATA provides nits
      amf_hdr->maxMasteringLuminance = static_cast<amf_uint32>(metadata->maxDisplayLuminance) * 10000;
      // minMasteringLuminance: both in 1/10000th of a nit
      amf_hdr->minMasteringLuminance = metadata->minDisplayLuminance;
      amf_hdr->maxContentLightLevel = metadata->maxContentLightLevel;
      amf_hdr->maxFrameAverageLightLevel = metadata->maxFrameAverageLightLevel;

      // Set HDR metadata on encoder
      const auto property_result = encoder->SetProperty(property, hdr_buffer);
      if (property_result != AMF_OK) {
        BOOST_LOG(warning) << "AMF: failed to apply HDR metadata, error: " << property_result;
        return false;
      }

      BOOST_LOG(info) << "AMF: HDR metadata set (max luminance: " << metadata->maxDisplayLuminance << " nits)";
      return true;
    }
  }

  void *
  amf_d3d11::get_input_texture() {
    return input_surface_ring[0].texture.Get();
  }

  ID3D11Texture2D *
  amf_d3d11::acquire_input_texture_for_render() {
    std::unique_lock lock(state_mutex);
    if (prepared_input_surface_slot) {
      return input_surface_ring[*prepared_input_surface_slot].texture.Get();
    }

    auto find_free_slot = [&]() -> std::optional<std::size_t> {
      for (std::size_t offset = 0; offset < active_input_surface_count; ++offset) {
        const auto slot = (next_input_surface_slot + offset) % active_input_surface_count;
        if (input_surface_ring[slot].state == input_surface_state_e::free) {
          return slot;
        }
      }
      return std::nullopt;
    };

    auto slot = find_free_slot();
    if (!slot && !output_fatal && active_input_surface_count < input_surface_ring.size()) {
      // Allocate outside the lock: a multi-MB CreateTexture2D under state_mutex
      // stalls the pump and AMF's release observers. Expansion only ever runs
      // on this thread, so re-checking after relock only has to account for
      // slots the observers freed meanwhile.
      lock.unlock();
      auto expansion_texture = create_input_surface_texture();
      lock.lock();
      slot = find_free_slot();
      if (!slot && expansion_texture && !output_fatal &&
          active_input_surface_count < input_surface_ring.size()) {
        auto &expansion_slot = input_surface_ring[active_input_surface_count];
        if (!expansion_slot.texture) {
          expansion_slot.texture = std::move(expansion_texture);
        }
        slot = active_input_surface_count;
        ++active_input_surface_count;
        BOOST_LOG(debug) << "AMF: expanded direct-render input pool to " << active_input_surface_count
                         << " surfaces during driver backlog";
      }
    }
    if (!slot) {
      state_cv.wait_for(lock, std::chrono::milliseconds(20), [&]() {
        return output_fatal || find_free_slot().has_value();
      });
      slot = find_free_slot();
    }
    if (!slot || output_fatal) {
      BOOST_LOG(error) << "AMF: no free direct-render input surface after bounded wait";
      return nullptr;
    }

    input_surface_ring[*slot].state = input_surface_state_e::reserved;
    input_surface_ring[*slot].frame_index = 0;
    input_surface_ring[*slot].release_notified = false;
    prepared_input_surface_slot = *slot;
    next_input_surface_slot = (*slot + 1) % active_input_surface_count;
    return input_surface_ring[*slot].texture.Get();
  }

  void
  amf_d3d11::cancel_input_texture_for_render() {
    std::lock_guard lock(state_mutex);
    if (!prepared_input_surface_slot) {
      return;
    }
    auto &slot = input_surface_ring[*prepared_input_surface_slot];
    if (slot.state == input_surface_state_e::reserved) {
      slot.state = input_surface_state_e::free;
      slot.frame_index = 0;
    }
    prepared_input_surface_slot.reset();
    state_cv.notify_all();
  }

  std::unique_ptr<amf_d3d11>
  create_amf_d3d11(ID3D11Device *d3d_device) {
    if (!d3d_device) return nullptr;

    auto enc = std::make_unique<amf_d3d11>(d3d_device);
    return enc;
  }

}  // namespace amf
