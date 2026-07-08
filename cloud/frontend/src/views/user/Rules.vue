<template>
  <div>
    <el-card shadow="hover">
      <template #header>
        <div style="display:flex;align-items:center;gap:8px">
          <el-icon size="20"><InfoFilled /></el-icon>
          <span style="font-weight:500">信用分规则说明</span>
        </div>
      </template>

      <div v-loading="loading">
        <!-- 扣分规则 -->
        <h4 style="color:#f56c6c;margin-bottom:12px">
          <el-icon><RemoveFilled /></el-icon> 扣分规则
        </h4>
        <el-table :data="penaltyList" stripe size="small" style="margin-bottom:24px">
          <el-table-column prop="category" label="违规行为" width="200" />
          <el-table-column prop="points" label="扣分" width="100">
            <template #default="{ row }"><span style="color:#f56c6c;font-weight:600">{{ row.points }}</span></template>
          </el-table-column>
          <el-table-column prop="description" label="说明" />
        </el-table>

        <!-- 加分规则 -->
        <h4 style="color:#67c23a;margin-bottom:12px">
          <el-icon><Plus /></el-icon> 加分规则
        </h4>
        <el-table :data="rewardList" stripe size="small" style="margin-bottom:24px">
          <el-table-column prop="action" label="行为" width="200" />
          <el-table-column prop="points" label="加分" width="100">
            <template #default="{ row }"><span style="color:#67c23a;font-weight:600">+{{ row.points }}</span></template>
          </el-table-column>
          <el-table-column prop="description" label="说明" />
        </el-table>

        <!-- 信用等级 -->
        <h4 style="margin-bottom:12px">
          <el-icon><TrendCharts /></el-icon> 信用等级说明
        </h4>
        <el-table :data="levelList" stripe size="small" style="margin-bottom:24px">
          <el-table-column prop="min" label="最低分" width="80" />
          <el-table-column prop="label" label="等级" width="120">
            <template #default="{ row }">
              <el-tag :color="levelColor(row.color)" size="small" style="color:#fff">{{ row.label }}</el-tag>
            </template>
          </el-table-column>
          <el-table-column prop="consequence" label="后果" />
        </el-table>

        <!-- 恢复机制 -->
        <h4 style="margin-bottom:12px">
          <el-icon><RefreshRight /></el-icon> 信用恢复机制
        </h4>
        <el-table :data="recoveryList" stripe size="small">
          <el-table-column prop="item" label="项目" width="200" />
          <el-table-column prop="detail" label="说明" />
        </el-table>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { authAPI } from '../../api'

const loading = ref(true)
const rules = ref(null)

const penaltyList = ref([])
const rewardList = ref([])
const levelList = ref([])
const recoveryList = ref([])

function levelColor(color) {
  const m = { green: '#67c23a', blue: '#409eff', yellow: '#e6a23c', orange: '#f56c6c', red: '#f5222d' }
  return m[color] || '#909399'
}

onMounted(async () => {
  loading.value = true
  try {
    const res = await authAPI.getCreditRules()
    rules.value = res

    penaltyList.value = Object.entries(res.penalties || {}).map(([k, v]) => ({
      category: v.description || k, points: v.points, description: v.description || ''
    }))

    rewardList.value = Object.entries(res.rewards || {}).map(([k, v]) => ({
      action: v.description || k, points: v.points, description: v.description || ''
    }))

    levelList.value = Object.entries(res.levels || {}).map(([k, v]) => ({
      ...v, color: v.color || 'gray'
    })).sort((a, b) => b.min - a.min)

    recoveryList.value = Object.entries(res.recovery || {}).map(([k, v]) => ({
      item: k, detail: v
    }))
  } catch (e) {}
  finally { loading.value = false }
})
</script>
