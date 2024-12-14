<script setup>
import {ref, computed, inject} from 'vue'
import {$tp} from '../../platform-i18n'
import PlatformLayout from '../../PlatformLayout.vue'
import AdapterNameSelector from './audiovideo/AdapterNameSelector.vue'
import DisplayOutputSelector from './audiovideo/DisplayOutputSelector.vue'
import DisplayDeviceOptions from "./audiovideo/DisplayDeviceOptions.vue";
import DisplayModesSettings from "./audiovideo/DisplayModesSettings.vue";

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

const validateFallbackMode = (event) => {
  const value = event.target.value;
  if (!value.match(/^\d+x\d+x\d+$/)) {
    event.target.setCustomValidity($t('config.fallback_mode_error'));
  } else {
    event.target.setCustomValidity('');
  }

  event.target.reportValidity();
}
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
        <div class="mb-3 form-check">
          <input type="checkbox" class="form-check-input" id="install_steam_audio_drivers" v-model="config.install_steam_audio_drivers" true-value="enabled" false-value="disabled"/>
          <label for="install_steam_audio_drivers" class="form-check-label">{{ $t('config.install_steam_audio_drivers') }}</label>
          <div class="form-text pre-wrap">{{ $t('config.install_steam_audio_drivers_desc') }}</div>
        </div>

        <div class="mb-3 form-check">
          <input type="checkbox" class="form-check-input" id="keep_sink_default" v-model="config.keep_sink_default" true-value="enabled" false-value="disabled"/>
          <label for="keep_sink_default" class="form-check-label">{{ $t('config.keep_sink_default') }}</label>
          <div class="form-text pre-wrap">{{ $t('config.keep_sink_default_desc') }}</div>
        </div>

        <div class="mb-3 form-check">
          <input type="checkbox" class="form-check-input" id="auto_capture_sink" v-model="config.auto_capture_sink" true-value="enabled" false-value="disabled"/>
          <label for="auto_capture_sink" class="form-check-label">{{ $t('config.auto_capture_sink') }}</label>
          <div class="form-text pre-wrap">{{ $t('config.auto_capture_sink_desc') }}</div>
        </div>
      </template>
    </PlatformLayout>


    <AdapterNameSelector
        :platform="platform"
        :config="config"
    />

    <DisplayOutputSelector
      :platform="platform"
      :config="config"
    />

    <!-- Display Modes -->
    <DisplayModesSettings
        :platform="platform"
        :config="config"
        :min_fps_factor="min_fps_factor"
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

    <div class="mb-3 form-check" v-if="platform === 'windows'">
      <input type="checkbox" class="form-check-input" id="follow_client_hdr" v-model="config.follow_client_hdr" true-value="enabled" false-value="disabled"/>
      <label for="follow_client_hdr" class="form-check-label">{{ $t('config.follow_client_hdr') }}</label>
      <div class="form-text pre-wrap">{{ $t('config.follow_client_hdr_desc') }}</div>
    </div>

    <!-- Headless Mode -->
    <div class="mb-3 form-check" v-if="platform === 'windows'">
      <input type="checkbox" class="form-check-input" id="headless_mode" v-model="config.headless_mode" true-value="enabled" false-value="disabled"/>
      <label for="headless_mode" class="form-check-label">{{ $t('config.headless_mode') }}</label>
      <div class="form-text">{{ $t('config.headless_mode_desc') }}</div>
    </div>

    <!-- Set VDisplay Primary -->
    <div class="mb-3 form-check" v-if="platform === 'windows'">
      <input type="checkbox" class="form-check-input" id="set_vdisplay_primary" v-model="config.set_vdisplay_primary" true-value="enabled" false-value="disabled"/>
      <label for="set_vdisplay_primary" class="form-check-label">{{ $t('config.set_vdisplay_primary') }}</label>
      <div class="form-text">{{ $t('config.set_vdisplay_primary_desc') }}</div>
    </div>

    <!-- SudoVDA Driver Status -->
    <div class="alert" :class="[vdisplay === '0' ? 'alert-success' : 'alert-warning']" v-if="platform === 'windows'">
      <i class="fa-solid fa-xl fa-circle-info"></i> SudoVDA Driver status: {{currentDriverStatus}}
    </div>
    <div class="form-text" v-if="vdisplay !== '0'">Please ensure SudoVDA driver is installed to the latest version and enabled properly.</div>

  </div>
</template>

<style scoped>
</style>
