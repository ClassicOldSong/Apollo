<script setup>
import {ref, computed, inject, onMounted, onBeforeUnmount} from 'vue'
import {$tp} from '../../platform-i18n'
import PlatformLayout from '../../PlatformLayout.vue'
import AdapterNameSelector from './audiovideo/AdapterNameSelector.vue'
import DisplayOutputSelector from './audiovideo/DisplayOutputSelector.vue'
import DisplayDeviceOptions from "./audiovideo/DisplayDeviceOptions.vue";
import DisplayModesSettings from "./audiovideo/DisplayModesSettings.vue";
import Checkbox from "../../Checkbox.vue";

const $t = inject('i18n').t;

const props = defineProps([
  'platform',
  'config',
  'vdisplay',
  'min_fps_factor',
])

const sudovdaStatus = {
  '1': 'Unknown',
  '0': 'Ready',
  '-1': 'Uninitialized',
  '-2': 'Version Incompatible',
  '-3': 'Watchdog Failed'
}

const currentDriverStatus = computed(() => sudovdaStatus[props.vdisplay])

const config = ref(props.config)
const micDebug = ref(null)
let micDebugPollHandle = null

const micMeterWidth = computed(() => `${Math.round(((micDebug.value?.lastInputLevel ?? 0) * 100))}%`)

const isFreshAge = (ageMs) => ageMs >= 0 && ageMs < 3000

const stageColorClass = (state) => ({
  success: 'bg-success-subtle border-success-subtle text-success-emphasis',
  warning: 'bg-warning-subtle border-warning-subtle text-warning-emphasis',
  danger: 'bg-danger-subtle border-danger-subtle text-danger-emphasis',
  idle: 'bg-secondary-subtle border-secondary-subtle text-secondary-emphasis',
}[state] || 'bg-secondary-subtle border-secondary-subtle text-secondary-emphasis')

const stageBadgeClass = (state) => ({
  success: 'text-bg-success',
  warning: 'text-bg-warning',
  danger: 'text-bg-danger',
  idle: 'text-bg-secondary',
}[state] || 'text-bg-secondary')

const micStages = computed(() => {
  const debug = micDebug.value
  if (!debug) {
    return []
  }

  const captureState = !debug.sessionActive ? 'idle' : (debug.firstPacketReceived ? 'success' : 'warning')
  const captureDetail = !debug.sessionActive
    ? 'Start a remote session with microphone redirection enabled.'
    : (debug.firstPacketReceived
      ? 'Moonlight is sending microphone audio to Apollo. Use the Moonlight preview bar to validate the local source.'
      : 'Apollo has negotiated microphone redirection, but Moonlight has not sent microphone audio yet.')

  let packetState = 'idle'
  let packetDetail = 'No active microphone session.'
  if (debug.sessionActive) {
    if (!debug.firstPacketReceived) {
      packetState = 'warning'
      packetDetail = 'Waiting for the first microphone packet from Moonlight.'
    } else if (isFreshAge(debug.lastPacketAgeMs)) {
      packetState = 'success'
      packetDetail = `Packets are arriving from Moonlight (${debug.lastPacketAgeMs} ms ago).`
    } else {
      packetState = 'warning'
      packetDetail = 'Packets arrived previously, but Apollo has not seen a fresh microphone packet recently.'
    }
  }

  let decodeState = 'idle'
  let decodeDetail = 'No decoded microphone audio on the host yet.'
  if (debug.sessionActive) {
    if (debug.decodeErrors > 0 && !debug.decodeActive) {
      decodeState = 'danger'
      decodeDetail = 'Apollo received microphone packets but could not decode them.'
    } else if (debug.decodeActive && isFreshAge(debug.lastDecodeAgeMs)) {
      decodeState = 'success'
      decodeDetail = `Apollo decoded microphone audio successfully (${debug.lastDecodeAgeMs} ms ago).`
    } else if (debug.packetsReceived > 0) {
      decodeState = 'warning'
      decodeDetail = 'Apollo is receiving packets, but decoded microphone audio has not been confirmed yet.'
    }
  }

  let renderState = 'idle'
  let renderDetail = 'VB-Cable rendering has not started.'
  if (debug.sessionActive) {
    if (debug.renderErrors > 0 && !debug.renderActive) {
      renderState = 'danger'
      renderDetail = 'Apollo decoded microphone audio, but writing it into VB-Cable failed.'
    } else if (debug.renderActive && isFreshAge(debug.lastRenderAgeMs)) {
      renderState = 'success'
      renderDetail = `Apollo is rendering microphone audio into ${debug.targetDeviceName || 'VB-Cable'} (${debug.lastRenderAgeMs} ms ago).`
    } else if (debug.decodeActive) {
      renderState = 'warning'
      renderDetail = 'Apollo decoded microphone audio, but the VB-Cable render stage has not completed yet.'
    }
  }

  let signalState = 'idle'
  let signalDetail = 'No decoded microphone signal is available yet.'
  if (debug.sessionActive) {
    if (debug.signalDetected) {
      signalState = 'success'
      signalDetail = 'Apollo is detecting non-silent microphone audio from Moonlight.'
    } else if (debug.decodeActive) {
      signalState = 'warning'
      signalDetail = 'Decoded microphone audio is currently silent or below the signal threshold.'
    } else if (debug.firstPacketReceived) {
      signalState = 'warning'
      signalDetail = 'Packets are arriving, but Apollo has not decoded usable microphone audio yet.'
    }
  }

  return [
    { key: 'capture', label: 'Moonlight capture/send', state: captureState, detail: captureDetail },
    { key: 'packets', label: 'Packets arriving', state: packetState, detail: packetDetail },
    { key: 'decode', label: 'Decoded on host', state: decodeState, detail: decodeDetail },
    { key: 'render', label: 'Rendered into VB-Cable', state: renderState, detail: renderDetail },
    { key: 'signal', label: 'Live signal detected', state: signalState, detail: signalDetail },
  ]
})

const loadMicDebug = async () => {
  if (props.platform !== 'windows') {
    return
  }

  try {
    const response = await fetch('./api/audio-debug', {
      credentials: 'include',
    })

    if (!response.ok) {
      return
    }

    micDebug.value = await response.json()
  } catch (e) {
    console.error('Failed to load microphone debug status', e)
  }
}

const validateFallbackMode = (event) => {
  const value = event.target.value;
  if (!value.match(/^\d+x\d+x\d+(\.\d+)?$/)) {
    event.target.setCustomValidity($t('config.fallback_mode_error'));
  } else {
    event.target.setCustomValidity('');
  }

  event.target.reportValidity();
}

onMounted(() => {
  loadMicDebug()

  if (props.platform === 'windows') {
    micDebugPollHandle = window.setInterval(loadMicDebug, 1000)
  }
})

onBeforeUnmount(() => {
  if (micDebugPollHandle !== null) {
    window.clearInterval(micDebugPollHandle)
    micDebugPollHandle = null
  }
})
</script>

<template>
  <div id="audio-video" class="config-page">
    <!-- Audio Sink -->
    <div class="mb-3">
      <label for="audio_sink" class="form-label">{{ $t('config.audio_sink') }}</label>
      <input type="text" class="form-control" id="audio_sink"
             :placeholder="$tp('config.audio_sink_placeholder', 'alsa_output.pci-0000_09_00.3.analog-stereo')"
             v-model="config.audio_sink" />
      <div class="form-text pre-wrap">
        {{ $tp('config.audio_sink_desc') }}<br>
        <PlatformLayout :platform="platform">
          <template #windows>
            <pre>tools\audio-info.exe</pre>
          </template>
          <template #linux>
            <pre>pacmd list-sinks | grep "name:"</pre>
            <pre>pactl info | grep Source</pre>
          </template>
          <template #macos>
            <a href="https://github.com/mattingalls/Soundflower" target="_blank">Soundflower</a><br>
            <a href="https://github.com/ExistentialAudio/BlackHole" target="_blank">BlackHole</a>.
          </template>
        </PlatformLayout>
      </div>
    </div>

    <PlatformLayout :platform="platform">
      <template #windows>
        <!-- Virtual Sink -->
        <div class="mb-3">
          <label for="virtual_sink" class="form-label">{{ $t('config.virtual_sink') }}</label>
          <input type="text" class="form-control" id="virtual_sink" :placeholder="$t('config.virtual_sink_placeholder')"
                 v-model="config.virtual_sink" />
          <div class="form-text pre-wrap">{{ $t('config.virtual_sink_desc') }}</div>
        </div>
        <!-- Install Steam Audio Drivers -->
        <Checkbox class="mb-3"
                  id="install_steam_audio_drivers"
                  locale-prefix="config"
                  v-model="config.install_steam_audio_drivers"
                  default="true"
        ></Checkbox>

        <Checkbox class="mb-3"
                  id="keep_sink_default"
                  locale-prefix="config"
                  v-model="config.keep_sink_default"
                  default="true"
        ></Checkbox>

        <Checkbox class="mb-3"
                  id="auto_capture_sink"
                  locale-prefix="config"
                  v-model="config.auto_capture_sink"
                  default="true"
        ></Checkbox>
      </template>
    </PlatformLayout>

    <!-- Disable Audio -->
    <Checkbox class="mb-3"
              id="stream_audio"
              locale-prefix="config"
              v-model="config.stream_audio"
              default="true"
    ></Checkbox>

    <Checkbox class="mb-3"
              id="stream_mic"
              locale-prefix="config"
              v-model="config.stream_mic"
              default="false"
    ></Checkbox>

    <div class="mb-3" v-if="platform === 'windows'">
      <label class="form-label">{{ $t('config.mic_backend') }}</label>
      <input type="text"
             class="form-control"
             :value="$t('config.mic_backend_apollo_virtual_mic')"
             disabled />
      <div class="form-text pre-wrap">{{ $t('config.mic_backend_desc_windows') }}</div>
    </div>

    <div class="mb-3" v-if="platform !== 'windows'">
      <label for="mic_device" class="form-label">{{ $t('config.mic_device') }}</label>
      <input type="text"
             class="form-control"
             id="mic_device"
             :placeholder="$tp('config.mic_device_placeholder', 'sunshine-mic')"
             v-model="config.mic_device" />
      <div class="form-text pre-wrap">
        {{ $tp('config.mic_device_desc') }}<br>
        <PlatformLayout :platform="platform">
          <template #windows>
            <pre>tools\audio-info.exe</pre>
          </template>
          <template #linux>
            <pre>pactl list short sinks</pre>
            <pre>pw-cli ls Node | grep -i sink</pre>
          </template>
          <template #macos>
            <a href="https://github.com/ExistentialAudio/BlackHole" target="_blank">BlackHole</a><br>
            <a href="https://rogueamoeba.com/loopback/" target="_blank">Loopback</a>
          </template>
        </PlatformLayout>
      </div>
    </div>

    <AdapterNameSelector
        :platform="platform"
        :config="config"
    />

    <DisplayOutputSelector
      :platform="platform"
      :config="config"
    />

    <DisplayDeviceOptions
      :platform="platform"
      :config="config"
    />

    <!-- Display Modes -->
    <DisplayModesSettings
        :platform="platform"
        :config="config"
    />

    <!-- Fallback Display Mode -->
    <div class="mb-3">
      <label for="fallback_mode" class="form-label">{{ $t('config.fallback_mode') }}</label>
      <input
        type="text"
        class="form-control"
        id="fallback_mode"
        v-model="config.fallback_mode"
        placeholder="1920x1080x60"
        @input="validateFallbackMode"
      />
      <div class="form-text">{{ $t('config.fallback_mode_desc') }}</div>
    </div>

    <!-- Headless Mode -->
    <Checkbox class="mb-3"
              id="headless_mode"
              locale-prefix="config"
              v-model="config.headless_mode"
              default="false"
              v-if="platform === 'windows'"
    ></Checkbox>

    <!-- Double Refreshrate -->
    <Checkbox class="mb-3"
              id="double_refreshrate"
              locale-prefix="config"
              v-model="config.double_refreshrate"
              default="false"
              v-if="platform === 'windows'"
    ></Checkbox>

    <!-- Isolated Virtual Display -->
    <Checkbox class="mb-3"
              id="isolated_virtual_display_option"
              locale-prefix="config"
              v-model="config.isolated_virtual_display_option"
              default="false"
              v-if="platform === 'windows'"
    ></Checkbox>

    <!-- SudoVDA Driver Status -->
    <div class="alert" :class="[vdisplay ? 'alert-warning' : 'alert-success']" v-if="platform === 'windows'">
      <i class="fa-solid fa-xl fa-circle-info"></i> SudoVDA Driver status: {{currentDriverStatus}}
    </div>
    <div class="form-text" v-if="platform === 'windows' && vdisplay">Please ensure SudoVDA driver is installed to the latest version and enabled properly.</div>

    <details class="mt-4" v-if="platform === 'windows' && micDebug">
      <summary class="fw-semibold">Remote Microphone Debug</summary>

      <div class="mt-3">
        <div class="alert" :class="micDebug.renderActive ? 'alert-success' : (micDebug.sessionActive ? 'alert-warning' : 'alert-secondary')">
          {{ micDebug.state || 'No active remote microphone session' }}
        </div>

        <div class="small text-muted mb-2">
          Use the Moonlight client preview to validate local microphone capture. The Apollo ladder below shows where the host-side path is succeeding or failing.
        </div>

        <div class="list-group mb-3">
          <div
            class="list-group-item border"
            :class="stageColorClass(stage.state)"
            v-for="stage in micStages"
            :key="stage.key"
          >
            <div class="d-flex justify-content-between align-items-center gap-3">
              <div class="fw-semibold">{{ stage.label }}</div>
              <span class="badge" :class="stageBadgeClass(stage.state)">
                {{ stage.state === 'success' ? 'OK' : (stage.state === 'warning' ? 'Waiting' : (stage.state === 'danger' ? 'Error' : 'Idle')) }}
              </span>
            </div>
            <div class="small mt-1">{{ stage.detail }}</div>
          </div>
        </div>

        <div class="row g-3">
          <div class="col-md-6">
            <div class="small text-muted mb-1">Client</div>
            <div>{{ micDebug.clientName || 'No active client' }}</div>

            <div class="small text-muted mt-3 mb-1">Backend</div>
            <div>{{ micDebug.backendName || 'Not initialized' }}</div>

            <div class="small text-muted mt-3 mb-1">Target device</div>
            <div>{{ micDebug.targetDeviceName || 'Unavailable' }}</div>

            <div class="small text-muted mt-3 mb-1">Packet flow</div>
            <div>Received: {{ micDebug.packetsReceived }}</div>
            <div>Decoded: {{ micDebug.packetsDecoded }}</div>
            <div>Rendered: {{ micDebug.packetsRendered }}</div>
            <div>Dropped: {{ micDebug.packetsDropped }}</div>
            <div>Decrypt errors: {{ micDebug.decryptErrors }}</div>
            <div>Decode errors: {{ micDebug.decodeErrors }}</div>
            <div>Render errors: {{ micDebug.renderErrors }}</div>
            <div>Silent packets: {{ micDebug.silentPackets }}</div>
          </div>

          <div class="col-md-6">
            <div class="small text-muted mb-1">Decoded microphone level</div>
            <div class="progress mb-2" role="progressbar" aria-label="Decoded microphone level">
              <div class="progress-bar" :class="micDebug.signalDetected ? 'bg-success' : 'bg-secondary'" :style="{ width: micMeterWidth }">
                {{ Math.round((micDebug.lastInputLevel || 0) * 100) }}%
              </div>
            </div>
            <div class="small text-muted">
              {{ micDebug.signalDetected ? 'Host is detecting non-silent microphone input from Moonlight.' : 'No non-silent microphone input detected yet.' }}
            </div>

            <div class="small text-muted mt-3 mb-1">Session state</div>
            <div>Mic requested: {{ micDebug.micRequested ? 'Yes' : 'No' }}</div>
            <div>Encryption: {{ micDebug.encryptionEnabled ? 'Enabled' : 'Disabled' }}</div>
            <div>First packet received: {{ micDebug.firstPacketReceived ? 'Yes' : 'No' }}</div>
            <div>Decode active: {{ micDebug.decodeActive ? 'Yes' : 'No' }}</div>
            <div>Last packet age: {{ micDebug.lastPacketAgeMs >= 0 ? `${micDebug.lastPacketAgeMs} ms` : 'N/A' }}</div>
            <div>Last decode age: {{ micDebug.lastDecodeAgeMs >= 0 ? `${micDebug.lastDecodeAgeMs} ms` : 'N/A' }}</div>
            <div>Last render age: {{ micDebug.lastRenderAgeMs >= 0 ? `${micDebug.lastRenderAgeMs} ms` : 'N/A' }}</div>
            <div>Last payload size: {{ micDebug.lastPayloadSize || 0 }} bytes</div>
            <div>Last sequence number: {{ micDebug.lastSequenceNumber || 0 }}</div>

            <div class="alert alert-danger mt-3 py-2" v-if="micDebug.lastError">
              {{ micDebug.lastError }}
            </div>
          </div>
        </div>

        <div class="mt-3">
          <div class="small text-muted mb-1">Recent microphone events</div>
          <ul class="list-group">
            <li class="list-group-item small" v-for="event in micDebug.recentEvents" :key="event">{{ event }}</li>
            <li class="list-group-item small text-muted" v-if="!micDebug.recentEvents || micDebug.recentEvents.length === 0">
              No microphone debug events yet.
            </li>
          </ul>
        </div>
      </div>
    </details>

  </div>
</template>

<style scoped>
</style>
