<template>
  <div class="layout">
    <nav class="nav">
      <router-link to="/">总览</router-link>
      <router-link to="/scenes">场景</router-link>
      <router-link to="/devices">设备</router-link>
      <router-link to="/ota">模型OTA</router-link>
    </nav>
    <main class="main"><router-view /></main>
  </div>
</template>
<style>
.layout{display:flex;min-height:100vh}
.nav{width:160px;background:#1e1e1e;padding:16px;display:flex;flex-direction:column;gap:12px}
.nav a{color:#ccc;text-decoration:none}
.nav a.router-link-active{color:#00C853}
.main{flex:1;padding:24px}
</style>
