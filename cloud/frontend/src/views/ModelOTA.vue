<template>
  <div>
    <h2>模型 OTA</h2>
    <button @click="check">检查更新 (scene_cnn)</button>
    <pre v-if="info">{{ info }}</pre>
  </div>
</template>
<script setup>
import { ref } from 'vue'
import axios from 'axios'
const info = ref('')
async function check() {
  const d = await axios.post('/api/ota/check', { device_id: 'dev-001', model_type: 'scene_cnn' })
  info.value = JSON.stringify(d.data, null, 2)
}
</script>
