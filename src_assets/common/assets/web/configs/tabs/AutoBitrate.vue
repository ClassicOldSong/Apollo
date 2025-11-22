<script setup>
import { ref, inject } from 'vue'
import Checkbox from '../../Checkbox.vue'

const $t = inject('i18n').t;

const props = defineProps([
  'platform',
  'config',
])

const config = ref(props.config)

// Validation functions
const validateMinBitrate = (event) => {
  const value = parseInt(event.target.value);
  const maxBitrate = config.value.auto_bitrate_max_bitrate || 150000;
  if (isNaN(value) || value < 100 || value > maxBitrate) {
    event.target.setCustomValidity(`Minimum bitrate must be between 100 and ${maxBitrate} kbps`);
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateMaxBitrate = (event) => {
  const value = parseInt(event.target.value);
  const minBitrate = config.value.auto_bitrate_min_bitrate || 500;
  if (isNaN(value) || value < minBitrate || value > 150000) {
    event.target.setCustomValidity(`Maximum bitrate must be between ${minBitrate} and 150000 kbps`);
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateThreshold = (event, min, max) => {
  const value = parseFloat(event.target.value);
  if (isNaN(value) || value < min || value > max) {
    event.target.setCustomValidity(`Value must be between ${min} and ${max}%`);
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateFactor = (event, min, max) => {
  const value = parseFloat(event.target.value);
  if (isNaN(value) || value < min || value > max) {
    event.target.setCustomValidity(`Value must be between ${min} and ${max}`);
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}
</script>

<template>
  <div id="auto-bitrate" class="config-page">
    <!-- Information Section -->
    <div class="alert alert-info mb-4">
      <h5 class="alert-heading">
        <i class="fa-solid fa-info-circle"></i> Auto Bitrate Feature
      </h5>
      <p class="mb-2">
        The auto bitrate feature dynamically adjusts video streaming bitrate based on network conditions 
        to maintain optimal streaming quality. When enabled, the system monitors frame loss statistics 
        and automatically adjusts bitrate to match network capabilities.
      </p>
      <hr>
      <p class="mb-0">
        <strong>How it works:</strong>
      </p>
      <ul class="mb-0">
        <li><strong>Poor Network</strong> (>{{ config.auto_bitrate_poor_network_threshold || 5.0 }}% frame loss): Bitrate is decreased by {{ ((1 - (config.auto_bitrate_decrease_factor || 0.8)) * 100).toFixed(0) }}%</li>
        <li><strong>Good Network</strong> (<{{ config.auto_bitrate_good_network_threshold || 1.0 }}% frame loss): After {{ config.auto_bitrate_min_consecutive_good_intervals || 3 }} consecutive good intervals and {{ ((config.auto_bitrate_stability_window_ms || 5000) / 1000) }}s stability, bitrate is increased by {{ (((config.auto_bitrate_increase_factor || 1.2) - 1) * 100).toFixed(0) }}%</li>
        <li><strong>Stable Network</strong> ({{ config.auto_bitrate_good_network_threshold || 1.0 }}-{{ config.auto_bitrate_poor_network_threshold || 5.0 }}% frame loss): Bitrate is maintained</li>
      </ul>
    </div>

    <!-- Note about client-side control -->
    <div class="alert alert-warning mb-4">
      <i class="fa-solid fa-exclamation-triangle"></i>
      <strong>Note:</strong> Auto bitrate must be enabled on the client side (Moonlight-qt) for this feature to work. 
      The client sends the auto bitrate preference during the RTSP handshake.
    </div>

    <!-- Algorithm Parameters -->
    <h5 class="mb-3">Algorithm Parameters</h5>

    <!-- Min Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_min_bitrate" class="form-label">
        Minimum Bitrate (kbps)
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_min_bitrate"
        v-model.number="config.auto_bitrate_min_bitrate"
        min="100"
        :max="config.auto_bitrate_max_bitrate || 150000"
        @input="validateMinBitrate"
      />
      <div class="form-text">
        Minimum bitrate that will be used when auto bitrate is active. Range: 100 - {{ config.auto_bitrate_max_bitrate || 150000 }} kbps
      </div>
    </div>

    <!-- Max Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_max_bitrate" class="form-label">
        Maximum Bitrate (kbps)
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_max_bitrate"
        v-model.number="config.auto_bitrate_max_bitrate"
        :min="config.auto_bitrate_min_bitrate || 500"
        max="150000"
        @input="validateMaxBitrate"
      />
      <div class="form-text">
        Maximum bitrate that will be used when auto bitrate is active. Range: {{ config.auto_bitrate_min_bitrate || 500 }} - 150000 kbps.
        This value is also limited by the global <code>max_bitrate</code> setting in the Audio/Video tab.
      </div>
    </div>

    <!-- Poor Network Threshold -->
    <div class="mb-3">
      <label for="auto_bitrate_poor_threshold" class="form-label">
        Poor Network Threshold (%)
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_poor_threshold"
        v-model.number="config.auto_bitrate_poor_network_threshold"
        min="1"
        max="50"
        step="0.1"
        @input="(e) => validateThreshold(e, 1, 50)"
      />
      <div class="form-text">
        Frame loss percentage above which the network is considered "poor" and bitrate will be decreased. Range: 1-50%
      </div>
    </div>

    <!-- Good Network Threshold -->
    <div class="mb-3">
      <label for="auto_bitrate_good_threshold" class="form-label">
        Good Network Threshold (%)
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_good_threshold"
        v-model.number="config.auto_bitrate_good_network_threshold"
        min="0.1"
        :max="config.auto_bitrate_poor_network_threshold || 5.0"
        step="0.1"
        @input="(e) => validateThreshold(e, 0.1, config.auto_bitrate_poor_network_threshold || 5.0)"
      />
      <div class="form-text">
        Frame loss percentage below which the network is considered "good" and bitrate may be increased. 
        Must be less than the poor network threshold. Range: 0.1-{{ config.auto_bitrate_poor_network_threshold || 5.0 }}%
      </div>
    </div>

    <!-- Increase Factor -->
    <div class="mb-3">
      <label for="auto_bitrate_increase_factor" class="form-label">
        Bitrate Increase Factor
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_increase_factor"
        v-model.number="config.auto_bitrate_increase_factor"
        min="1.01"
        max="2.0"
        step="0.01"
        @input="(e) => validateFactor(e, 1.01, 2.0)"
      />
      <div class="form-text">
        Multiplier applied when increasing bitrate on good network conditions. 
        Example: 1.2 means a 20% increase. Range: 1.01-2.0
      </div>
    </div>

    <!-- Decrease Factor -->
    <div class="mb-3">
      <label for="auto_bitrate_decrease_factor" class="form-label">
        Bitrate Decrease Factor
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_decrease_factor"
        v-model.number="config.auto_bitrate_decrease_factor"
        min="0.1"
        max="0.99"
        step="0.01"
        @input="(e) => validateFactor(e, 0.1, 0.99)"
      />
      <div class="form-text">
        Multiplier applied when decreasing bitrate on poor network conditions. 
        Example: 0.8 means a 20% decrease. Range: 0.1-0.99
      </div>
    </div>

    <!-- Stability Window -->
    <div class="mb-3">
      <label for="auto_bitrate_stability_window" class="form-label">
        Stability Window (milliseconds)
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_stability_window"
        v-model.number="config.auto_bitrate_stability_window_ms"
        min="1000"
        max="30000"
        step="1000"
      />
      <div class="form-text">
        Time window (in milliseconds) that network conditions must remain good before increasing bitrate. 
        Range: 1000-30000 ms (1-30 seconds). Default: 5000 ms (5 seconds)
      </div>
    </div>

    <!-- Min Consecutive Good Intervals -->
    <div class="mb-3">
      <label for="auto_bitrate_min_good_intervals" class="form-label">
        Minimum Consecutive Good Intervals
      </label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_min_good_intervals"
        v-model.number="config.auto_bitrate_min_consecutive_good_intervals"
        min="1"
        max="10"
      />
      <div class="form-text">
        Number of consecutive reporting intervals with good network conditions required before increasing bitrate. 
        Range: 1-10 intervals
      </div>
    </div>

    <!-- Reset to Defaults Button -->
    <div class="mb-3">
      <button 
        type="button" 
        class="btn btn-secondary"
        @click="() => {
          config.auto_bitrate_min_bitrate = 500;
          config.auto_bitrate_max_bitrate = 150000;
          config.auto_bitrate_poor_network_threshold = 5.0;
          config.auto_bitrate_good_network_threshold = 1.0;
          config.auto_bitrate_increase_factor = 1.2;
          config.auto_bitrate_decrease_factor = 0.8;
          config.auto_bitrate_stability_window_ms = 5000;
          config.auto_bitrate_min_consecutive_good_intervals = 3;
        }"
      >
        <i class="fa-solid fa-rotate-left"></i> Reset to Defaults
      </button>
    </div>
  </div>
</template>

<style scoped>
.alert {
  margin-bottom: 1rem;
}

.alert ul {
  margin-bottom: 0;
  padding-left: 1.5rem;
}

code {
  background-color: rgba(0, 0, 0, 0.05);
  padding: 2px 4px;
  border-radius: 3px;
  font-size: 0.9em;
}
</style>

