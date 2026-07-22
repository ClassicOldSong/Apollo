<script setup>
import { ref } from 'vue'
import Checkbox from "../../../Checkbox.vue";

const props = defineProps([
  'platform',
  'config',
])

const config = ref(props.config)
</script>

<template>
  <div id="amd-amf-encoder" class="config-page">
    <!-- AMF Usage -->
    <div class="mb-3">
      <label for="amd_usage" class="form-label">{{ $t('config.amd_usage') }}</label>
      <select id="amd_usage" class="form-select" v-model="config.amd_usage">
        <option value="transcoding">{{ $t('config.amd_usage_transcoding') }}</option>
        <option value="webcam">{{ $t('config.amd_usage_webcam') }}</option>
        <option value="lowlatency_high_quality">{{ $t('config.amd_usage_lowlatency_high_quality') }}</option>
        <option value="lowlatency">{{ $t('config.amd_usage_lowlatency') }}</option>
        <option value="ultralowlatency">{{ $t('config.amd_usage_ultralowlatency') }}</option>
      </select>
      <div class="form-text">{{ $t('config.amd_usage_desc') }}</div>
    </div>

    <!-- AMD Rate Control group options -->
    <div class="mb-3 accordion">
      <div class="accordion-item">
        <h2 class="accordion-header">
          <button class="accordion-button" type="button" data-bs-toggle="collapse"
                  data-bs-target="#panelsStayOpen-collapseOne">
            {{ $t('config.amd_rc_group') }}
          </button>
        </h2>
        <div id="panelsStayOpen-collapseOne" class="accordion-collapse collapse show"
             aria-labelledby="panelsStayOpen-headingOne">
          <div class="accordion-body">
            <!-- AMF Rate Control -->
            <div class="mb-3">
              <label for="amd_rc" class="form-label">{{ $t('config.amd_rc') }}</label>
              <select id="amd_rc" class="form-select" v-model="config.amd_rc">
                <option value="cbr">{{ $t('config.amd_rc_cbr') }}</option>
                <option value="cqp">{{ $t('config.amd_rc_cqp') }}</option>
                <option value="vbr_latency">{{ $t('config.amd_rc_vbr_latency') }}</option>
                <option value="vbr_peak">{{ $t('config.amd_rc_vbr_peak') }}</option>
                <option value="qvbr">{{ $t('config.amd_rc_qvbr') }}</option>
                <option value="hqvbr">{{ $t('config.amd_rc_hqvbr') }}</option>
                <option value="hqcbr">{{ $t('config.amd_rc_hqcbr') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_rc_desc') }}</div>
            </div>

            <!-- AMF QVBR quality level -->
            <div class="mb-3">
              <label for="amd_qvbr_quality_level" class="form-label">{{ $t('config.amd_qvbr_quality_level') }}</label>
              <input type="number" min="0" max="51" class="form-control" id="amd_qvbr_quality_level"
                     v-model="config.amd_qvbr_quality_level" />
              <div class="form-text">{{ $t('config.amd_qvbr_quality_level_desc') }}</div>
            </div>

            <!-- AMF HRD Enforcement -->
            <Checkbox class="mb-3"
                      id="amd_enforce_hrd"
                      locale-prefix="config"
                      v-model="config.amd_enforce_hrd"
                      default="false"
            ></Checkbox>
          </div>
        </div>
      </div>
    </div>

    <!-- AMF Quality group options -->
    <div class="mb-3 accordion">
      <div class="accordion-item">
        <h2 class="accordion-header">
          <button class="accordion-button" type="button" data-bs-toggle="collapse"
                  data-bs-target="#panelsStayOpen-collapseTwo">
            {{ $t('config.amd_quality_group') }}
          </button>
        </h2>
        <div id="panelsStayOpen-collapseTwo" class="accordion-collapse collapse show"
             aria-labelledby="panelsStayOpen-headingTwo">
          <div class="accordion-body">
            <!-- AMF Quality -->
            <div class="mb-3">
              <label for="amd_quality" class="form-label">{{ $t('config.amd_quality') }}</label>
              <select id="amd_quality" class="form-select" v-model="config.amd_quality">
                <option value="speed">{{ $t('config.amd_quality_speed') }}</option>
                <option value="balanced">{{ $t('config.amd_quality_balanced') }}</option>
                <option value="quality">{{ $t('config.amd_quality_quality') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_quality_desc') }}</div>
            </div>

            <!-- AMD Preanalysis -->
            <Checkbox class="mb-3"
                      id="amd_preanalysis"
                      locale-prefix="config"
                      v-model="config.amd_preanalysis"
                      default="false"
            ></Checkbox>

            <!-- AMD VBAQ -->
            <Checkbox class="mb-3"
                      id="amd_vbaq"
                      locale-prefix="config"
                      v-model="config.amd_vbaq"
                      default="true"
            ></Checkbox>

            <!-- AMF Coder (H264) -->
            <div class="mb-3">
              <label for="amd_coder" class="form-label">{{ $t('config.amd_coder') }}</label>
              <select id="amd_coder" class="form-select" v-model="config.amd_coder">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="cabac">{{ $t('config.coder_cabac') }}</option>
                <option value="cavlc">{{ $t('config.coder_cavlc') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_coder_desc') }}</div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Native AMF advanced options -->
    <div class="mb-3 accordion">
      <div class="accordion-item">
        <h2 class="accordion-header">
          <button class="accordion-button collapsed" type="button" data-bs-toggle="collapse"
                  data-bs-target="#amd-native-advanced">
            {{ $t('config.amd_native_group') }}
          </button>
        </h2>
        <div id="amd-native-advanced" class="accordion-collapse collapse">
          <div class="accordion-body">
            <div class="mb-3">
              <label for="amd_ltr_frames" class="form-label">{{ $t('config.amd_ltr_frames') }}</label>
              <input type="number" min="0" max="2" class="form-control" id="amd_ltr_frames"
                     v-model="config.amd_ltr_frames" />
              <div class="form-text">{{ $t('config.amd_ltr_frames_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_input_queue_size" class="form-label">{{ $t('config.amd_input_queue_size') }}</label>
              <input type="number" min="0" max="32" class="form-control" id="amd_input_queue_size"
                     v-model="config.amd_input_queue_size" />
              <div class="form-text">{{ $t('config.amd_input_queue_size_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_smart_access_video" class="form-label">{{ $t('config.amd_smart_access_video') }}</label>
              <select id="amd_smart_access_video" class="form-select" v-model="config.amd_smart_access_video">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="enabled">{{ $t('config.enabled') }}</option>
                <option value="disabled">{{ $t('config.disabled') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_smart_access_video_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_lowlatency_mode" class="form-label">{{ $t('config.amd_lowlatency_mode') }}</label>
              <select id="amd_lowlatency_mode" class="form-select" v-model="config.amd_lowlatency_mode">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="enabled">{{ $t('config.enabled') }}</option>
                <option value="disabled">{{ $t('config.disabled') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_lowlatency_mode_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_high_motion_quality_boost" class="form-label">{{ $t('config.amd_high_motion_quality_boost') }}</label>
              <select id="amd_high_motion_quality_boost" class="form-select" v-model="config.amd_high_motion_quality_boost">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="enabled">{{ $t('config.enabled') }}</option>
                <option value="disabled">{{ $t('config.disabled') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_high_motion_quality_boost_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_av1_screen_content" class="form-label">{{ $t('config.amd_av1_screen_content') }}</label>
              <select id="amd_av1_screen_content" class="form-select" v-model="config.amd_av1_screen_content">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="enabled">{{ $t('config.enabled') }}</option>
                <option value="disabled">{{ $t('config.disabled') }}</option>
              </select>
              <div class="form-text">{{ $t('config.amd_av1_screen_content_desc') }}</div>
            </div>

            <div class="mb-3">
              <label for="amd_av1_latency_mode" class="form-label">{{ $t('config.amd_av1_latency_mode') }}</label>
              <select id="amd_av1_latency_mode" class="form-select" v-model="config.amd_av1_latency_mode">
                <option value="auto">{{ $t('config.ffmpeg_auto') }}</option>
                <option value="none">none</option>
                <option value="power_saving">power_saving</option>
                <option value="realtime">realtime</option>
                <option value="lowest">lowest</option>
              </select>
              <div class="form-text">{{ $t('config.amd_av1_latency_mode_desc') }}</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>

</style>
