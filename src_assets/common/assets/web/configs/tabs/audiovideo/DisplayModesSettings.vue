<script setup>
import { ref } from 'vue'
import { $tp } from '../../../platform-i18n'
import PlatformLayout from '../../../PlatformLayout.vue'

const props = defineProps([
  'platform',
  'config',
  'vdisplay',
  'min_fps_factor',
])

const config = ref(props.config)

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
  </div>
</template>
