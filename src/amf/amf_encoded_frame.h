/**
 * @file src/amf/amf_encoded_frame.h
 * @brief Declarations for AMF encoded frame.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace amf {

  /**
   * @brief Encoded frame from AMF encoder.
   */
  struct amf_encoded_frame {
    std::vector<uint8_t> data;  ///< Encoded bitstream payload.
    uint64_t frame_index = 0;  ///< Input frame index carried through AMF PTS.
    bool idr = false;  ///< Whether the payload is an IDR or key frame.
    bool after_ref_frame_invalidation = false;  ///< Whether this frame follows RFI recovery.
    bool fatal = false;  ///< Whether the encoder entered an unrecoverable state.
  };

  /**
   * @brief Result of one native AMF submission attempt.
   *
   * Output can contain frames from earlier submissions because AMF is asynchronous.
   * input_accepted is kept separate so recovery state (IDR/RFI/LTR) is committed only
   * after the current surface was actually accepted by the encoder.
   */
  struct amf_encode_result {
    std::vector<amf_encoded_frame> frames;  ///< Ready frames in presentation order.
    bool input_accepted = false;  ///< Whether AMF accepted the current input surface.
    bool fatal = false;  ///< Whether the encoder entered an unrecoverable state.
  };

}  // namespace amf
