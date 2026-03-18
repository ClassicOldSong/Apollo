/**
 * @file src/platform/windows/mic_write.cpp
 * @brief Windows microphone redirection writer.
 */
#include "mic_write.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <string_view>
#include <vector>

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <opus/opus.h>

#include "misc.h"
#include "src/audio.h"
#include "src/config.h"
#include "src/logging.h"

namespace platf::audio {
  namespace {
    constexpr PROPERTYKEY PKEY_Device_DeviceDesc {
      {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
      2
    };
    constexpr PROPERTYKEY PKEY_Device_FriendlyName {
      {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
      14
    };
    constexpr PROPERTYKEY PKEY_DeviceInterface_FriendlyName {
      {0x026e516e, 0xb814, 0x414b, {0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22}},
      2
    };

    template<typename T>
    void co_task_free(T *ptr) {
      if (ptr) {
        CoTaskMemFree(ptr);
      }
    }

    using device_t = util::safe_ptr<IMMDevice, release_com<IMMDevice>>;
    using collection_t = util::safe_ptr<IMMDeviceCollection, release_com<IMMDeviceCollection>>;
    using prop_t = util::safe_ptr<IPropertyStore, release_com<IPropertyStore>>;
    using wstring_t = util::safe_ptr<WCHAR, co_task_free<WCHAR>>;

    class prop_var_t {
    public:
      prop_var_t() {
        PropVariantInit(&value);
      }

      ~prop_var_t() {
        PropVariantClear(&value);
      }

      PROPVARIANT value;
    };

    std::wstring get_prop_string(IPropertyStore *prop, REFPROPERTYKEY key) {
      prop_var_t value;
      if (FAILED(prop->GetValue(key, &value.value)) || value.value.vt != VT_LPWSTR || value.value.pwszVal == nullptr) {
        return {};
      }

      return value.value.pwszVal;
    }

    bool contains_case_insensitive(std::wstring haystack, std::wstring needle) {
      std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::towlower);
      std::transform(needle.begin(), needle.end(), needle.begin(), ::towlower);
      return haystack.find(needle) != std::wstring::npos;
    }

    bool is_recoverable_device_error(HRESULT status) {
      return status == AUDCLNT_E_DEVICE_INVALIDATED ||
             status == AUDCLNT_E_RESOURCES_INVALIDATED ||
             status == AUDCLNT_E_SERVICE_NOT_RUNNING;
    }

    bool recover_device(mic_write_wasapi_t &writer, HRESULT status, const char *operation) {
      if (!is_recoverable_device_error(status)) {
        return false;
      }

      BOOST_LOG(warning) << "Microphone playback device needs reinitialization after failure while " << operation
                         << ": 0x" << util::hex(status).to_string_view();

      writer.cleanup();
      return writer.init() == 0;
    }
  }  // namespace

  mic_write_wasapi_t::mic_write_wasapi_t(std::string backend_name,
                                         std::vector<std::wstring> autodetect_patterns,
                                         std::string requested_device_name):
      backend_name {std::move(backend_name)},
      requested_device_name {std::move(requested_device_name)},
      autodetect_patterns {std::move(autodetect_patterns)} {
  }

  mic_write_wasapi_t::~mic_write_wasapi_t() {
    cleanup();
  }

  std::string_view mic_write_wasapi_t::backend_id() const {
    return backend_name;
  }

  bool mic_write_wasapi_t::find_target_device(std::wstring &device_id, std::string &device_name) {
    collection_t collection;
    HRESULT status = device_enum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(status) || !collection) {
      BOOST_LOG(error) << "Couldn't enumerate render devices for microphone redirection: 0x" << util::hex(status).to_string_view();
      return false;
    }

    std::wstring requested_name = requested_device_name.empty() ? std::wstring {} : from_utf8(requested_device_name);
    auto patterns = autodetect_patterns;
    if (patterns.empty()) {
      patterns = {
        L"CABLE Input",
        L"VB-Audio Virtual Cable",
      };
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT index = 0; index < count; ++index) {
      device_t device;
      if (FAILED(collection->Item(index, &device)) || !device) {
        continue;
      }

      wstring_t id;
      if (FAILED(device->GetId(&id)) || !id) {
        continue;
      }

      prop_t prop;
      if (FAILED(device->OpenPropertyStore(STGM_READ, &prop)) || !prop) {
        continue;
      }

      auto friendly_name = get_prop_string(prop.get(), PKEY_Device_FriendlyName);
      auto interface_name = get_prop_string(prop.get(), PKEY_DeviceInterface_FriendlyName);
      auto description = get_prop_string(prop.get(), PKEY_Device_DeviceDesc);

      if (requested_name.empty() &&
          (contains_case_insensitive(friendly_name, L"16ch") ||
           contains_case_insensitive(interface_name, L"16ch") ||
           contains_case_insensitive(description, L"16ch"))) {
        continue;
      }

      bool matched = false;
      if (!requested_name.empty()) {
        matched = friendly_name == requested_name || interface_name == requested_name || description == requested_name || id.get() == requested_name;
      } else {
        for (const auto &pattern : patterns) {
          if (contains_case_insensitive(friendly_name, pattern) ||
              contains_case_insensitive(interface_name, pattern) ||
              contains_case_insensitive(description, pattern)) {
            matched = true;
            break;
          }
        }
      }

      if (!matched) {
        continue;
      }

      device_id = id.get();
      device_name = to_utf8(!friendly_name.empty() ? friendly_name : interface_name);
      return true;
    }

    return false;
  }

  bool mic_write_wasapi_t::initialize_device() {
    std::wstring device_id;
    if (!find_target_device(device_id, target_device_name)) {
      if (requested_device_name.empty()) {
        BOOST_LOG(warning) << "No supported VB-CABLE playback device found. Install VB-CABLE and ensure \"CABLE Input\" is available.";
        ::audio::mic_debug_on_backend_error("VB-CABLE was not found on the host. Install VB-CABLE and ensure CABLE Input is available.");
      } else {
        BOOST_LOG(warning) << "Requested microphone device not found: " << requested_device_name;
        ::audio::mic_debug_on_backend_error("Requested microphone render device was not found: " + requested_device_name);
      }
      return false;
    }

    device_t device;
    HRESULT status = device_enum->GetDevice(device_id.c_str(), &device);
    if (FAILED(status) || !device) {
      BOOST_LOG(error) << "Couldn't open microphone playback device [" << target_device_name << "]: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not open host microphone render device [" + target_device_name + "]");
      return false;
    }

    status = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **) &audio_client);
    if (FAILED(status) || !audio_client) {
      BOOST_LOG(error) << "Couldn't activate microphone playback client [" << target_device_name << "]: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not activate the host microphone playback client for [" + target_device_name + "]");
      return false;
    }

    std::array<WAVEFORMATEX, 4> formats {{
      {WAVE_FORMAT_PCM, 1, 48000, 96000, 2, 16, 0},
      {WAVE_FORMAT_PCM, 2, 48000, 192000, 4, 16, 0},
      {WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0},
      {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0},
    }};

    HRESULT init_status = E_FAIL;
    constexpr REFERENCE_TIME buffer_duration_100ns = 1000000;
    for (const auto &format : formats) {
      init_status = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        buffer_duration_100ns,
        0,
        &format,
        nullptr
      );
      if (SUCCEEDED(init_status)) {
        active_format = format;
        break;
      }
    }

    if (FAILED(init_status)) {
      BOOST_LOG(error) << "Couldn't initialize microphone playback client [" << target_device_name << "]: 0x" << util::hex(init_status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not initialize the host microphone playback client for [" + target_device_name + "]");
      return false;
    }

    status = audio_client->GetBufferSize(&buffer_frame_count);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Couldn't query microphone playback buffer size: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not query the microphone playback buffer size");
      return false;
    }

    status = audio_client->GetService(IID_IAudioRenderClient, (void **) &audio_render);
    if (FAILED(status) || !audio_render) {
      BOOST_LOG(error) << "Couldn't acquire microphone playback render client: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not acquire the microphone playback render client");
      return false;
    }

    BOOST_LOG(info) << "Client microphone redirection target: " << target_device_name
                    << " (" << active_format.nChannels << "ch @" << active_format.nSamplesPerSec << "Hz)";
    ::audio::mic_debug_on_backend_target(target_device_name, active_format.nChannels, active_format.nSamplesPerSec);

    status = audio_client->Start();
    if (FAILED(status)) {
      BOOST_LOG(error) << "Couldn't start microphone playback client: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not start the microphone playback client");
      return false;
    }

    return true;
  }

  int mic_write_wasapi_t::init() {
    int opus_error = OPUS_OK;
    opus_decoder = opus_decoder_create(48000, 1, &opus_error);
    if (opus_error != OPUS_OK || opus_decoder == nullptr) {
      BOOST_LOG(error) << "Couldn't create Opus decoder for microphone redirection: " << opus_strerror(opus_error);
      ::audio::mic_debug_on_backend_error("Could not create the Opus decoder for microphone redirection");
      return -1;
    }

    HRESULT status = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **) &device_enum);
    if (FAILED(status) || !device_enum) {
      BOOST_LOG(error) << "Couldn't create device enumerator for microphone redirection: 0x" << util::hex(status).to_string_view();
      ::audio::mic_debug_on_backend_error("Could not create the Windows audio device enumerator for microphone redirection");
      return -1;
    }

    if (!initialize_device()) {
      return -1;
    }

    ::audio::mic_debug_on_backend_initialized(std::string {backend_name});

    return 0;
  }

  int mic_write_wasapi_t::write_data(const char *data, std::size_t len, std::uint16_t sequence_number) {
    if (!audio_client || audio_render == nullptr || opus_decoder == nullptr || data == nullptr || len == 0) {
      BOOST_LOG(warning) << "Client microphone packet rejected before decode because the WASAPI write path is not ready"
                         << " [seq=" << sequence_number
                         << ", len=" << len
                         << ", audio_client=" << static_cast<bool>(audio_client)
                         << ", audio_render=" << static_cast<bool>(audio_render != nullptr)
                         << ", opus_decoder=" << static_cast<bool>(opus_decoder != nullptr)
                         << ", data=" << static_cast<bool>(data != nullptr) << ']';
      return -1;
    }

    std::array<opus_int16, 960> mono_pcm {};
    const auto decoded_frames = opus_decode(opus_decoder, reinterpret_cast<const unsigned char *>(data), static_cast<opus_int32>(len), mono_pcm.data(), static_cast<int>(mono_pcm.size()), 0);
    if (decoded_frames <= 0) {
      BOOST_LOG(debug) << "Couldn't decode microphone Opus frame";
      ::audio::mic_debug_on_decode_error(sequence_number, "The host could not decode the incoming Opus microphone frame");
      return -1;
    }

    int peak = 0;
    for (int i = 0; i < decoded_frames; ++i) {
      const int sample = mono_pcm[i] < 0 ? -mono_pcm[i] : mono_pcm[i];
      peak = std::max(peak, sample);
    }
    const double normalized_level = static_cast<double>(peak) / 32767.0;
    const bool silent = peak < 512;
    ::audio::mic_debug_on_packet_decoded(sequence_number, normalized_level, silent);

    std::vector<opus_int16> output_pcm;
    output_pcm.reserve(decoded_frames * active_format.nChannels);
    if (active_format.nChannels == 1) {
      output_pcm.insert(output_pcm.end(), mono_pcm.begin(), mono_pcm.begin() + decoded_frames);
    } else {
      for (int i = 0; i < decoded_frames; ++i) {
        output_pcm.push_back(mono_pcm[i]);
        output_pcm.push_back(mono_pcm[i]);
      }
    }

    const auto bytes_per_frame = static_cast<std::size_t>(active_format.nBlockAlign);
    UINT32 frames_written = 0;
    int empty_buffer_waits = 0;
    while (frames_written < static_cast<UINT32>(decoded_frames)) {
      UINT32 padding = 0;
      auto status = audio_client->GetCurrentPadding(&padding);
      if (FAILED(status)) {
        if (recover_device(*this, status, "querying microphone playback padding")) {
          return 0;
        }
        ::audio::mic_debug_on_render_error(sequence_number, "Could not query microphone playback padding");
        return -1;
      }

      if (padding > buffer_frame_count) {
        padding = 0;
      }

      auto frames_available = buffer_frame_count - padding;
      if (frames_available == 0) {
        if (++empty_buffer_waits > 8) {
          break;
        }
        Sleep(5);
        continue;
      }

      const auto frames_to_write = std::min<UINT32>(frames_available, static_cast<UINT32>(decoded_frames) - frames_written);
      if (frames_to_write == 0) {
        break;
      }

      BYTE *buffer = nullptr;
      status = audio_render->GetBuffer(frames_to_write, &buffer);
      if (FAILED(status) || buffer == nullptr) {
        if (FAILED(status) && recover_device(*this, status, "acquiring a microphone playback buffer")) {
          return 0;
        }
        BOOST_LOG(debug) << "Couldn't acquire microphone playback buffer for [" << target_device_name << "]: 0x"
                         << util::hex(status).to_string_view();
        ::audio::mic_debug_on_render_error(sequence_number, "Could not acquire a microphone playback buffer from Windows");
        return -1;
      }

      std::memcpy(buffer, output_pcm.data() + (frames_written * active_format.nChannels), frames_to_write * bytes_per_frame);
      status = audio_render->ReleaseBuffer(frames_to_write, 0);
      if (FAILED(status)) {
        if (recover_device(*this, status, "releasing a microphone playback buffer")) {
          return 0;
        }
        BOOST_LOG(debug) << "Couldn't release microphone playback buffer for [" << target_device_name << "]: 0x"
                         << util::hex(status).to_string_view();
        ::audio::mic_debug_on_render_error(sequence_number, "Could not release a microphone playback buffer to Windows");
        return -1;
      }

      frames_written += frames_to_write;
      empty_buffer_waits = 0;
    }

    if (frames_written == 0) {
      ::audio::mic_debug_on_render_error(sequence_number, "The microphone playback buffer stayed full long enough that this packet was skipped");
      return 0;
    }

    if (frames_written < static_cast<UINT32>(decoded_frames)) {
      BOOST_LOG(debug) << "Microphone playback buffer filled before the whole packet could be rendered for ["
                       << target_device_name << "], wrote " << frames_written << " of " << decoded_frames << " frames";
    }

    if (!first_packet_written_logged) {
      first_packet_written_logged = true;
      BOOST_LOG(info) << "Client microphone audio is being rendered into [" << target_device_name << ']';
    }

    ::audio::mic_debug_on_packet_rendered(sequence_number, normalized_level, silent);

    return decoded_frames;
  }

  void mic_write_wasapi_t::cleanup() {
    if (audio_client) {
      audio_client->Stop();
    }

    if (audio_render != nullptr) {
      audio_render->Release();
      audio_render = nullptr;
    }

    audio_client.reset();
    device_enum.reset();

    if (opus_decoder != nullptr) {
      opus_decoder_destroy(opus_decoder);
      opus_decoder = nullptr;
    }

    buffer_frame_count = 0;
    active_format = {};
    target_device_name.clear();
    first_packet_written_logged = false;
  }
}  // namespace platf::audio
