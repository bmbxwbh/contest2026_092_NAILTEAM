<template>
  <div>
    <h2>总览</h2>
    <div class="cards">
      <div class="card"><h3>设备数</h3><p>{{ deviceCount }}</p></div>
      <div class="card"><h3>今日场景上报</h3><p>{{ todayCount }}</p></div>
      <div class="card"><h3>当前模型版本</h3><p>{{ modelVer }}</p></div>
    </div>
    <h3>场景分布</h3>
    <div v-for="s in dist" :key="s.scene" class="bar">
      <span>{{ s.scene }}</span>
      <div class="fill" :style="{width: (s.count*100/maxCount)+'%'}"></div>
      <span>{{ s.count }}</span>
    </div>
  </div>
</template>
<script setup>
import { ref, onMounted } from 'vue'
import axios from 'axios'
const deviceCount = ref(0), todayCount = ref(0), modelVer = ref('-'), dist = ref([]), maxCount = ref(1)
onMounted(async () => {
  const d = await axios.get('/api/stats/scene_distribution', { params: { device_id: 'dev-001' } })
  dist.value = d.data
  maxCount.value = Math.max(...d.data.map(x=>x.count), 1)
})
</script>
<style>
.cards{display:flex;gap:16px;margin-bottom:24px}
.card{background:#f5f5f5;padding:16px;border-radius:8px;min-width:160px}
.bar{display:flex;align-items:center;gap:8px;margin:4px 0}
.fill{height:16px;background:#00C853;border-radius:4px}
</style>
