<template>
  <div>
    <el-card shadow="hover">
      <template #header>
        <span style="font-weight: 500">预警发送记录</span>
      </template>

      <el-table :data="alerts" stripe v-loading="loading">
        <el-table-column prop="id" label="ID" width="60" />
        <el-table-column label="预警类型" width="90">
          <template #default="{ row }">
            <el-tag :type="alertTypeTag(row.alert_type)" size="small">{{ alertTypeLabel(row.alert_type) }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="title" label="标题" width="160" />
        <el-table-column prop="message" label="内容" min-width="200" show-overflow-tooltip />
        <el-table-column label="目标用户" width="120">
          <template #default="{ row }">{{ row.admin_name || '-' }}</template>
        </el-table-column>
        <el-table-column label="发送者" width="120">
          <template #default="{ row }">{{ row.admin_name || '系统' }}</template>
        </el-table-column>
        <el-table-column label="已读" width="70">
          <template #default="{ row }">
            <el-tag :type="row.is_read ? 'info' : 'primary'" size="small">{{ row.is_read ? '是' : '否' }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="created_at" label="发送时间" width="180">
          <template #default="{ row }">{{ formatTime(row.created_at) }}</template>
        </el-table-column>
      </el-table>

      <div class="pagination-wrapper">
        <el-pagination
          v-model:current-page="page"
          :page-size="perPage"
          :total="total"
          layout="prev, pager, next, total"
          @current-change="fetchAlerts"
        />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { adminAPI } from '../../api'

const alerts = ref([])
const total = ref(0)
const page = ref(1)
const perPage = ref(20)
const loading = ref(false)

function formatTime(t) { return t ? new Date(t).toLocaleString('zh-CN') : '' }
function alertTypeTag(t) { const m={warning:'warning',reminder:'',urgent:'danger',system:'info'}; return m[t]||'info' }
function alertTypeLabel(t) { const m={warning:'警告',reminder:'提醒',urgent:'紧急',system:'系统'}; return m[t]||t }

async function fetchAlerts() {
  loading.value = true
  try {
    const res = await adminAPI.getAlerts({ page: page.value, per_page: perPage.value })
    alerts.value = res.alerts
    total.value = res.total
  } catch (e) {} finally {
    loading.value = false
  }
}

onMounted(fetchAlerts)
</script>

<style scoped>
.pagination-wrapper {
  margin-top: 20px;
  display: flex;
  justify-content: flex-end;
}
</style>
