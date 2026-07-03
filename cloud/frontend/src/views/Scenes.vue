<template>
  <div>
    <h2>场景记录</h2>
    <table>
      <tr><th>场景</th><th>置信度</th><th>时间</th></tr>
      <tr v-for="(r,i) in rows" :key="i"><td>{{ r.scene }}</td><td>{{ (r.confidence*100).toFixed(0) }}%</td><td>{{ r.ts }}</td></tr>
    </table>
  </div>
</template>
<script setup>
import { ref, onMounted } from 'vue'
import axios from 'axios'
const rows = ref([])
onMounted(async () => {
  const d = await axios.get('/api/scenes/list', { params: { device_id: 'dev-001', limit: 50 } })
  rows.value = d.data
})
</script>
<style>
table{border-collapse:collapse;width:100%}
th,td{border:1px solid #ddd;padding:8px;text-align:left}
th{background:#fafafa}
</style>
