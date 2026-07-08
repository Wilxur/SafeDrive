<template>
  <el-container style="height: 100vh">
    <el-aside :width="isCollapse ? '64px' : '220px'" class="app-aside">
      <div class="logo" @click="goHome">
        <el-icon :size="28" color="#409eff"><Van /></el-icon>
        <span v-show="!isCollapse" class="logo-text">货车信用平台</span>
      </div>

      <el-menu
        :default-active="route.path"
        :collapse="isCollapse"
        :collapse-transition="false"
        background-color="#001529"
        text-color="#ffffffa6"
        active-text-color="#fff"
        router
      >
        <template v-if="auth.isAdmin">
          <el-menu-item index="/admin">
            <el-icon><Odometer /></el-icon>
            <span>运营看板</span>
          </el-menu-item>
          <el-menu-item index="/admin/users">
            <el-icon><UserFilled /></el-icon>
            <span>司机管理</span>
          </el-menu-item>
          <el-menu-item index="/admin/alerts">
            <el-icon><BellFilled /></el-icon>
            <span>预警管理</span>
          </el-menu-item>
        </template>

        <template v-else>
          <el-menu-item index="/dashboard">
            <el-icon><Odometer /></el-icon>
            <span>驾驶状态</span>
          </el-menu-item>
          <el-menu-item index="/records">
            <el-icon><List /></el-icon>
            <span>检测记录</span>
          </el-menu-item>
          <el-menu-item index="/alerts">
            <el-icon><BellFilled /></el-icon>
            <span>预警提醒</span>
            <el-badge v-if="unreadCount > 0" :value="unreadCount" class="menu-badge" />
          </el-menu-item>
          <el-menu-item index="/rules">
            <el-icon><InfoFilled /></el-icon>
            <span>信用规则</span>
          </el-menu-item>
          <el-menu-item index="/profile">
            <el-icon><User /></el-icon>
            <span>个人信息</span>
          </el-menu-item>
        </template>
      </el-menu>
    </el-aside>

    <el-container>
      <el-header class="app-header">
        <div class="header-left">
          <el-icon :size="20" class="collapse-btn" @click="isCollapse = !isCollapse">
            <Fold v-if="!isCollapse" />
            <Expand v-else />
          </el-icon>
          <span class="page-title">{{ route.meta?.title || '' }}</span>
        </div>
        <div class="header-right">
          <el-dropdown trigger="click">
            <span class="user-info">
              <el-avatar :size="32" icon="UserFilled" />
              <span class="username">{{ auth.username }}</span>
              <el-tag :type="auth.isAdmin ? 'danger' : 'primary'" size="small">
                {{ auth.isAdmin ? '物流管理' : '司机' }}
              </el-tag>
            </span>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item @click="$router.push('/profile')">
                  <el-icon><User /></el-icon>个人信息
                </el-dropdown-item>
                <el-dropdown-item divided @click="handleLogout">
                  <el-icon><SwitchButton /></el-icon>退出登录
                </el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </div>
      </el-header>

      <el-main class="app-main">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '../store/auth'
import { alertsAPI } from '../api'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
const isCollapse = ref(false)
const unreadCount = ref(0)

function goHome() {
  router.push(auth.isAdmin ? '/admin' : '/dashboard')
}

async function fetchUnreadCount() {
  try {
    if (!auth.isAdmin) {
      const res = await alertsAPI.getMyAlerts({ unread_only: 1, per_page: 1 })
      unreadCount.value = res.unread_count || 0
    }
  } catch (e) {}
}

function handleLogout() {
  auth.logout()
  router.push('/login')
}

onMounted(() => {
  fetchUnreadCount()
  setInterval(fetchUnreadCount, 30000)
})
</script>

<style scoped>
.app-aside { background-color: #001529; overflow: hidden; transition: width 0.3s; }
.logo { height: 60px; display: flex; align-items: center; justify-content: center; gap: 10px; border-bottom: 1px solid #ffffff1a; cursor: pointer; }
.logo-text { color: #fff; font-size: 16px; font-weight: bold; white-space: nowrap; }
.el-menu { border-right: none; }
.app-header { background: #fff; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid #e4e7ed; padding: 0 20px; height: 60px; }
.header-left { display: flex; align-items: center; gap: 12px; }
.collapse-btn { cursor: pointer; }
.page-title { font-size: 16px; font-weight: 500; }
.header-right { display: flex; align-items: center; }
.user-info { display: flex; align-items: center; gap: 8px; cursor: pointer; }
.username { font-size: 14px; }
.app-main { background: #f5f7fa; padding: 20px; overflow-y: auto; }
.menu-badge { margin-left: auto; }
</style>