<script setup>
const props = defineProps({
  platform: {
    type: String,
    required: true
  }
})
</script>

<template>
  <slot name="windows" v-if="$slots.windows && platform === 'windows'"></slot>
  <slot name="linux" v-if="$slots.linux && platform === 'linux'"></slot>
  <slot name="macos" v-if="$slots.macos && platform === 'macos'"></slot>
</template>


<style scoped>

</style>
