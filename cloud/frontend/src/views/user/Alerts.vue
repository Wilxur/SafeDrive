<template>
  <div class="page-bg">
    <!-- 统计卡片 -->
    <el-row :gutter="16" style="margin-bottom:16px">
      <el-col :span="6">
        <el-card shadow="hover" class="alert-stat-card glass-card">
          <div class="alert-stat-value primary">{{ total }}</div>
          <div class="alert-stat-label">总预警数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="alert-stat-card glass-card">
          <div class="alert-stat-value danger">{{ unreadTotal }}</div>
          <div class="alert-stat-label">未读预警</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="alert-stat-card glass-card">
          <div class="alert-stat-value warning">{{ urgentCount }}</div>
          <div class="alert-stat-label">紧急预警</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="alert-stat-card glass-card">
          <div class="alert-stat-value success">{{ readCount }}</div>
          <div class="alert-stat-label">已处理</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 筛选和操作栏 -->
    <el-card shadow="hover" class="glass-card" style="margin-bottom:16px">
      <div style="display:flex;justify-content:space-between;align-items:center">
        <div style="display:flex;gap:12px;align-items:center">
          <el-select v-model="filterType" placeholder="预警类型" clearable style="width:120px" @change="fetchAlerts">
            <el-option label="全部" value="" />
            <el-option label="警告" value="warning" />
            <el-option label="提醒" value="reminder" />
            <el-option label="紧急" value="urgent" />
            <el-option label="系统" value="system" />
          </el-select>
          <el-select v-model="filterRead" placeholder="阅读状态" clearable style="width:120px" @change="fetchAlerts">
            <el-option label="全部" value="" />
            <el-option label="未读" value="unread" />
            <el-option label="已读" value="read" />
          </el-select>
          <el-button size="small" @click="refresh">刷新</el-button>
        </div>
        <el-button v-if="unreadTotal > 0" type="primary" size="small" @click="markAllRead">全部标记已读</el-button>
      </div>
    </el-card>

    <!-- 预警列表 -->
    <el-card shadow="hover" class="glass-card">
      <div v-if="alerts.length === 0" style="text-align:center;padding:60px;color:#909399">
        <el-icon :size="48" color="#d9d9d9"><BellFilled /></el-icon>
        <div style="margin-top:12px">暂无预警消息</div>
      </div>
      <div v-else class="alert-list">
        <div
          v-for="item in alerts"
          :key="item.id"
          class="alert-item"
          :class="{ 'alert-unread': !item.is_read }"
          @click="markRead(item)"
        >
          <div class="alert-icon">
            <el-icon :size="24" :color="alertIconColor(item.alert_type)">
              <component :is="alertIcon(item.alert_type)" />
            </el-icon>
          </div>
          <div class="alert-body">
            <div class="alert-header">
              <span class="alert-title">{{ item.title }}</span>
              <div class="alert-meta">
                <el-tag :type="alertTagType(item.alert_type)" size="small">{{ alertTypeLabel(item.alert_type) }}</el-tag>
                <span class="alert-time">{{ formatTime(item.created_at) }}</span>
              </div>
            </div>
            <div class="alert-message">{{ item.message }}</div>
          </div>
          <div class="alert-status">
            <el-tag v-if="!item.is_read" type="danger" size="small" effect="dark">新</el-tag>
          </div>
        </div>
      </div>

      <div v-if="total > perPage" class="pagination-wrapper">
        <el-pagination
          v-model:current-page="page"
          :page-size="perPage"
          :total="total"
          layout="prev, pager, next"
          @current-change="fetchAlerts"
          small
        />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { alertsAPI } from '../../api'
import { ElMessage } from 'element-plus'

const alerts = ref([])
const total = ref(0)
const page = ref(1)
const perPage = ref(20)
const unreadTotal = ref(0)
const filterType = ref('')
const filterRead = ref('')

const urgentCount = computed(() => alerts.value.filter(a => a.alert_type === 'urgent').length)
const readCount = computed(() => alerts.value.filter(a => a.is_read).length)

function alertIcon(type) {
  const icons = { warning: 'WarningFilled', reminder: 'BellFilled', urgent: 'RemoveFilled', system: 'InfoFilled' }
  return icons[type] || 'BellFilled'
}

function alertIconColor(type) {
  const colors = { warning: '#e6a23c', reminder: '#409eff', urgent: '#f56c6c', system: '#909399' }
  return colors[type] || '#409eff'
}

function alertTagType(type) {
  const types = { warning: 'warning', reminder: 'primary', urgent: 'danger', system: 'info' }
  return types[type] || 'info'
}

function alertTypeLabel(type) {
  const labels = { warning: '警告', reminder: '提醒', urgent: '紧急', system: '系统' }
  return labels[type] || type
}

function formatTime(t) { return t ? new Date(t).toLocaleString('zh-CN') : '' }

async function fetchAlerts() {
  try {
    const params = { page: page.value, per_page: perPage.value }
    if (filterType.value) params.type = filterType.value
    if (filterRead.value === 'unread') params.unread_only = 1
    const res = await alertsAPI.getMyAlerts(params)
    alerts.value = res.alerts || []
    total.value = res.total || 0
    unreadTotal.value = res.unread_count || 0
  } catch (e) {}
}

async function markRead(item) {
  if (item.is_read) return
  try {
    await alertsAPI.markAsRead(item.id)
    item.is_read = 1
    unreadTotal.value = Math.max(0, unreadTotal.value - 1)
  } catch (e) {}
}

async function markAllRead() {
  try {
    await alertsAPI.markAllAsRead()
    alerts.value.forEach(a => { a.is_read = 1 })
    unreadTotal.value = 0
    ElMessage.success('已全部标记为已读')
  } catch (e) {}
}

function refresh() { page.value = 1; fetchAlerts() }

onMounted(fetchAlerts)
</script>

<style scoped>
.alert-stat-card { text-align: center; }
.alert-stat-value { font-size: 32px; font-weight: bold; }
.alert-stat-value.primary { color: #409eff; }
.alert-stat-value.danger { color: #f56c6c; }
.alert-stat-value.warning { color: #e6a23c; }
.alert-stat-value.success { color: #52c41a; }
.alert-stat-label { font-size: 13px; color: #909399; margin-top: 4px; }
.glass-card { background: rgba(255,255,255,0.92) !important; backdrop-filter: blur(10px); }

.alert-list { display: flex; flex-direction: column; gap: 8px; }
.alert-item {
  display: flex; gap: 16px; align-items: flex-start;
  padding: 16px; border-radius: 8px;
  background: rgba(255,255,255,0.6);
  border: 1px solid rgba(0,0,0,0.06);
  cursor: pointer;
  transition: all 0.2s;
}
.alert-item:hover { background: rgba(64,158,255,0.04); border-color: rgba(64,158,255,0.2); }
.alert-unread { background: rgba(64,158,255,0.06); border-left: 3px solid #409eff; }
.alert-icon { flex-shrink: 0; margin-top: 2px; }
.alert-body { flex: 1; min-width: 0; }
.alert-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; }
.alert-title { font-weight: 500; font-size: 14px; color: #333; }
.alert-meta { display: flex; align-items: center; gap: 8px; }
.alert-time { font-size: 12px; color: #bbb; }
.alert-message { font-size: 13px; color: #606266; line-height: 1.6; }
.alert-status { flex-shrink: 0; }
.pagination-wrapper { margin-top: 20px; display: flex; justify-content: center; }
</style>