<template>
  <div>
    <h2>设备</h2>
    <form @submit.prevent="register">
      <input v-model="deviceId" placeholder="设备ID" />
      <input v-model="name" placeholder="名称" />
      <button>注册</button>
    </form>
    <pre v-if="token">{{ token }}</pre>
  </div>
</template>
<script setup>
import { ref } from 'vue'
import axios from 'axios'
const deviceId = ref(''), name = ref(''), token = ref('')
async function register() {
  const d = await axios.post('/api/devices/register', { device_id: deviceId.value, name: name.value })
  token.value = d.data.token
}
</script>
