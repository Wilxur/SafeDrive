<template>
  <div>
    <!-- 顶部统计卡片 -->
    <el-row :gutter="16">
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#e6f7ff"><el-icon :size="24" color="#1890ff"><UserFilled /></el-icon></div>
          <div class="stat-body"><div class="stat-value">{{ dashboard.total_users }}</div><div class="stat-label">司机总数</div></div>
        </el-card>
      </el-col>
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#fff7e6"><el-icon :size="24" color="#fa8c16"><WarningFilled /></el-icon></div>
          <div class="stat-body"><div class="stat-value danger">{{ dashboard.today_dangerous }}</div><div class="stat-label">今日危险行为</div></div>
        </el-card>
      </el-col>
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#f6ffed"><el-icon :size="24" color="#52c41a"><Odometer /></el-icon></div>
          <div class="stat-body"><div class="stat-value">{{ dashboard.today_detections }}</div><div class="stat-label">今日检测总数</div></div>
        </el-card>
      </el-col>
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#e6f7ff"><el-icon :size="24" color="#1890ff"><UserFilled /></el-icon></div>
          <div class="stat-body"><div class="stat-value primary">{{ dashboard.active_users_today }}</div><div class="stat-label">今日活跃</div></div>
        </el-card>
      </el-col>
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#fff0f0"><el-icon :size="24" color="#f5222d"><RemoveFilled /></el-icon></div>
          <div class="stat-body"><div class="stat-value danger">{{ dashboard.blacklisted_count }}</div><div class="stat-label">已拉黑</div></div>
        </el-card>
      </el-col>
      <el-col :span="4">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background:#f0f5ff"><el-icon :size="24" color="#722ed1"><BellFilled /></el-icon></div>
          <div class="stat-body"><div class="stat-value warning">{{ dashboard.unread_alerts }}</div><div class="stat-label">未读预警</div></div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 第二行：平均信用分 -->
    <el-row :gutter="16" style="margin-top:16px">
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">信用分概况</span></template>
          <div class="credit-overview">
            <div class="credit-circle">
              <div class="credit-score-num">{{ dashboard.average_credit }}</div>
              <div class="credit-score-label">全员均分</div>
            </div>
            <div class="credit-detail">
              <div class="credit-bar-item" v-for="item in creditLevels" :key="item.key">
                <span class="credit-bar-label">
                  <span class="dot" :style="{ background: item.color }"></span>{{ item.label }}
                </span>
                <div class="credit-bar-track">
                  <div class="credit-bar-fill" :style="{ width: creditPercent(item.key) + '%', background: item.color }"></div>
                </div>
                <span class="credit-bar-count">{{ dashboard.credit_distribution?.[item.key] || 0 }}人</span>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="8">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">违规种类饼状图</span></template>
          <div class="pie-container">
            <canvas ref="pieCanvas" width="220" height="220"></canvas>
            <div class="pie-legend">
              <div v-for="item in pieData" :key="item.key" class="legend-item">
                <span class="legend-dot" :style="{ background: item.color }"></span>
                <span class="legend-label">{{ item.label }}</span>
                <span class="legend-value">{{ item.value }}</span>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="8">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">物流公司信用排行</span></template>
          <div v-if="dashboard.companies_rank?.length">
            <div v-for="(c, i) in dashboard.companies_rank" :key="c.id" class="company-rank-row">
              <span class="rank-badge" :class="'rank-' + (i+1)">{{ i+1 }}</span>
              <span class="company-name">{{ c.name }}</span>
              <div class="company-score-bar">
                <div class="score-fill" :style="{ width: c.avg_credit + '%', background: creditColor(c.avg_credit) }"></div>
              </div>
              <span class="company-score" :style="{ color: creditColor(c.avg_credit) }">{{ c.avg_credit }}</span>
              <span class="company-drivers">{{ c.driver_count }}人</span>
            </div>
          </div>
          <el-empty v-else description="暂无公司" />
        </el-card>
      </el-col>
    </el-row>

    <!-- 第四行：用户信用排行 -->
    <el-row :gutter="16" style="margin-top:16px">
      <el-col :span="24">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">司机信用排行</span></template>
          <div v-loading="rankingLoading" style="max-height:360px;overflow-y:auto">
            <el-table :data="userRanking" stripe size="small" v-if="userRanking.length">
              <el-table-column label="排名" width="60">
                <template #default="{ row }">
                  <span class="rank-badge-inline" :class="'rank-' + row.rank" v-if="row.rank <= 3">{{ row.rank }}</span>
                  <span v-else style="color:#999;font-size:12px">{{ row.rank }}</span>
                </template>
              </el-table-column>
              <el-table-column prop="real_name" label="姓名" width="100" />
              <el-table-column prop="company_name" label="公司" width="140" />
              <el-table-column prop="license_plate" label="车牌" width="120" />
              <el-table-column prop="credit_score" label="信用分" width="90">
                <template #default="{ row }">
                  <span :style="{ color: creditColor(row.credit_score), fontWeight: 600 }">{{ row.credit_score }}</span>
                </template>
              </el-table-column>
              <el-table-column prop="total_trips" label="出车" width="70" />
              <el-table-column prop="violation_count" label="违规" width="70" />
              <el-table-column label="状态" width="80">
                <template #default="{ row }">
                  <el-tag v-if="row.blacklisted" type="danger" size="small">已拉黑</el-tag>
                  <el-tag v-else type="success" size="small">正常</el-tag>
                </template>
              </el-table-column>
            </el-table>
            <el-empty v-else description="暂无司机数据" :image-size="60" />
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 第三行：历史柱状图 -->
    <el-row :gutter="16" style="margin-top:16px">
      <el-col :span="24">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">近7天危险驾驶类别分布</span></template>
          <div ref="chartRef" style="height:280px"></div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { ref, onMounted, nextTick, computed } from 'vue'
import { adminAPI } from '../../api'

const dashboard = ref({
  total_users: 0, today_dangerous: 0, today_detections: 0,
  active_users_today: 0, unread_alerts: 0, blacklisted_count: 0,
  total_companies: 0, average_credit: 0,
  credit_distribution: { excellent: 0, good: 0, fair: 0, poor: 0, critical: 0 },
  companies_rank: [],
  hourly_dangerous: {},
  weekly_category_distribution: {},
})
const chartRef = ref(null)
const pieCanvas = ref(null)
const userRanking = ref([])
const rankingLoading = ref(false)

const creditLevels = [
  { key: 'excellent', label: '优秀 (90-100)', color: '#52c41a' },
  { key: 'good', label: '良好 (70-89)', color: '#1890ff' },
  { key: 'fair', label: '一般 (50-69)', color: '#fa8c16' },
  { key: 'poor', label: '较差 (30-49)', color: '#f5222d' },
  { key: 'critical', label: '危险 (0-29)', color: '#722ed1' },
]

const pieData = computed(() => {
  const dist = dashboard.value.weekly_category_distribution || {}
  const labels = { yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带', phone: '使用手机', hands_off: '双手离方向盘' }
  const colors = ['#f89898', '#f56c6c', '#e6a23c', '#909399', '#67c23a']
  return Object.entries(dist).map(([key, value], i) => ({
    key, value,
    label: labels[key] || key,
    color: colors[i] || '#999'
  })).filter(d => d.value > 0)
})

function creditPercent(key) {
  const total = Object.values(dashboard.value.credit_distribution || {}).reduce((a, b) => a + b, 0)
  if (!total) return 0
  return ((dashboard.value.credit_distribution?.[key] || 0) / total) * 100
}

function creditColor(score) {
  if (score >= 90) return '#52c41a'
  if (score >= 70) return '#1890ff'
  if (score >= 50) return '#fa8c16'
  if (score >= 30) return '#f5222d'
  return '#722ed1'
}

function drawPieChart() {
  const canvas = pieCanvas.value
  if (!canvas) return
  const ctx = canvas.getContext('2d')
  const cx = 110, cy = 110, r = 100
  ctx.clearRect(0, 0, 220, 220)

  const data = pieData.value
  if (data.length === 0) {
    ctx.fillStyle = '#f0f0f0'
    ctx.beginPath()
    ctx.arc(cx, cy, r, 0, Math.PI * 2)
    ctx.fill()
    ctx.fillStyle = '#999'
    ctx.font = '14px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText('暂无数据', cx, cy + 5)
    return
  }

  const total = data.reduce((s, d) => s + d.value, 0)
  let startAngle = -Math.PI / 2

  data.forEach(item => {
    const sliceAngle = (item.value / total) * Math.PI * 2
    ctx.beginPath()
    ctx.moveTo(cx, cy)
    ctx.arc(cx, cy, r, startAngle, startAngle + sliceAngle)
    ctx.closePath()
    ctx.fillStyle = item.color
    ctx.fill()
    startAngle += sliceAngle
  })

  // 中心白点
  ctx.beginPath()
  ctx.arc(cx, cy, 45, 0, Math.PI * 2)
  ctx.fillStyle = '#fff'
  ctx.fill()

  ctx.fillStyle = '#333'
  ctx.font = 'bold 18px sans-serif'
  ctx.textAlign = 'center'
  ctx.fillText(total, cx, cy + 5)
}

function renderBarChart() {
  const container = chartRef.value
  if (!container) return
  const data = dashboard.value.weekly_category_distribution || {}
  const labels = { yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带', phone: '使用手机', hands_off: '双手离方向盘' }
  const colors = ['#f89898', '#f56c6c', '#e6a23c', '#909399', '#67c23a']
  const max = Math.max(...Object.values(data), 1)

  let html = '<div style="display:flex;align-items:flex-end;height:230px;gap:20px;padding:20px 40px">'
  Object.entries(data).forEach(([key, value], i) => {
    const h = (value / max) * 210
    html += `<div style="flex:1;display:flex;flex-direction:column;align-items:center;gap:6px">`
    html += `<span style="font-size:15px;font-weight:600;color:${colors[i]}">${value}</span>`
    html += `<div style="width:65px;height:${Math.max(h, 2)}px;background:linear-gradient(180deg,${colors[i]},${colors[i]}cc);border-radius:6px 6px 0 0;transition:height 0.3s"></div>`
    html += `<span style="font-size:12px;color:#606266">${labels[key]}</span>`
    html += `</div>`
  })
  html += '</div>'
  container.innerHTML = html
}

onMounted(async () => {
  try {
    const res = await adminAPI.getDashboard()
    dashboard.value = res
    await nextTick()
    renderBarChart()
    drawPieChart()
  } catch (e) {}
})
</script>

<style scoped>
.stat-card {
  display: flex; align-items: center; padding: 4px;
}
.stat-card :deep(.el-card__body) {
  display: flex; align-items: center; gap: 12px; width: 100%;
}
.stat-icon {
  width: 48px; height: 48px; border-radius: 12px;
  display: flex; align-items: center; justify-content: center; flex-shrink: 0;
}
.stat-body { flex: 1; min-width: 0; }
.stat-value { font-size: 26px; font-weight: bold; color: #333; line-height: 1.2; }
.stat-value.danger { color: #f56c6c; }
.stat-value.primary { color: #409eff; }
.stat-value.warning { color: #e6a23c; }
.stat-label { font-size: 12px; color: #909399; margin-top: 2px; }

/* 信用分概况 */
.credit-overview {
  display: flex; gap: 24px; align-items: center;
}
.credit-circle {
  width: 100px; height: 100px; border-radius: 50%;
  background: linear-gradient(135deg, #1890ff, #52c41a);
  display: flex; flex-direction: column; align-items: center;
  justify-content: center; flex-shrink: 0;
}
.credit-score-num { font-size: 32px; font-weight: bold; color: #fff; line-height: 1; }
.credit-score-label { font-size: 11px; color: rgba(255,255,255,0.85); margin-top: 4px; }
.credit-detail { flex: 1; }
.credit-bar-item { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
.credit-bar-label { font-size: 12px; color: #606266; width: 90px; display: flex; align-items: center; gap: 4px; }
.dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
.credit-bar-track { flex: 1; height: 10px; background: #f0f0f0; border-radius: 5px; overflow: hidden; }
.credit-bar-fill { height: 100%; border-radius: 5px; transition: width 0.3s; }
.credit-bar-count { font-size: 12px; color: #909399; width: 35px; text-align: right; }

/* 饼图 */
.pie-container { display: flex; gap: 20px; align-items: center; }
.pie-legend { display: flex; flex-direction: column; gap: 6px; }
.legend-item { display: flex; align-items: center; gap: 6px; font-size: 12px; }
.legend-dot { width: 10px; height: 10px; border-radius: 2px; flex-shrink: 0; }
.legend-label { color: #606266; flex: 1; }
.legend-value { color: #333; font-weight: 500; }

/* 公司排行 */
.company-rank-row { display: flex; align-items: center; gap: 8px; padding: 6px 0; }
.rank-badge { width: 20px; height: 20px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 11px; font-weight: 600; color: #fff; flex-shrink: 0; }
.rank-badge.rank-1 { background: #f5222d; }
.rank-badge.rank-2 { background: #fa8c16; }
.rank-badge.rank-3 { background: #1890ff; }
.rank-badge:not(.rank-1):not(.rank-2):not(.rank-3) { background: #d9d9d9; color: #666; }
.company-name { width: 80px; font-size: 13px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.company-score-bar { flex: 1; height: 8px; background: #f0f0f0; border-radius: 4px; overflow: hidden; }
.score-fill { height: 100%; border-radius: 4px; transition: width 0.3s; }
.company-score { font-size: 12px; font-weight: 600; width: 35px; text-align: right; }
.company-drivers { font-size: 11px; color: #909399; width: 35px; text-align: right; }
</style>