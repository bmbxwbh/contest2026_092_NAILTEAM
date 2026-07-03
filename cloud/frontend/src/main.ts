import { createApp } from 'vue'
import { createRouter, createWebHistory } from 'vue-router'
import App from './App.vue'
import Dashboard from './views/Dashboard.vue'
import Scenes from './views/Scenes.vue'
import Devices from './views/Devices.vue'
import ModelOTA from './views/ModelOTA.vue'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', component: Dashboard },
    { path: '/scenes', component: Scenes },
    { path: '/devices', component: Devices },
    { path: '/ota', component: ModelOTA },
  ],
})
createApp(App).use(router).mount('#app')
