<script setup>
import { ref, computed } from 'vue'
import { $tp } from '../../../platform-i18n'
import PlatformLayout from '../../../PlatformLayout.vue'

const props = defineProps([
  'platform',
  'config',
  'vdisplay',
  'min_fps_factor',
])

const config = ref(props.config)

const sudovdaStatus = {
  '1': 'Unknown',
  '0': 'Ready',
  '-1': 'Uninitialized',
  '-2': 'Version Incompatible',
  '-3': 'Watchdog Failed'
}

const currentDriverStatus = computed(() => sudovdaStatus[props.vdisplay])

const resIn = ref("")
const fpsIn = ref("")
</script>

<template>
  <div class="mb-3">
    <!--min_fps_factor-->
    <div class="mb-3">
      <label for="qp" class="form-label">{{ $t('config.min_fps_factor') }}</label>
      <input type="number" min="1" max="3" class="form-control" id="min_fps_factor" placeholder="1" v-model="config.min_fps_factor" />
      <div class="form-text">{{ $t('config.min_fps_factor_desc') }}</div>
    </div>

    <div class="alert" :class="[vdisplay === '0' ? 'alert-success' : 'alert-warning']">
      <label><i class="fa-solid fa-xl fa-circle-info"></i> SudoVDA Driver status: {{currentDriverStatus}}</label>
    </div>
    <div class="form-text" v-if="vdisplay !== '0'">Please ensure SudoVDA driver is installed to the latest version and enabled properly.</div>
  </div>
</template>

<style scoped>
.ms-item {
  background-color: var(--bs-dark-bg-subtle);
  font-size: 12px;
  font-weight: bold;
}
</style>
