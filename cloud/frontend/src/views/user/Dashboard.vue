<template>
  <div class="dashboard">
    <!-- 信用分卡片 -->
    <el-row :gutter="16" style="margin-bottom:16px">
      <el-col :span="24">
        <el-card shadow="hover" class="credit-banner">
          <div class="credit-banner-inner">
            <div class="credit-score-display">
              <div class="cs-label">我的信用分</div>
              <div class="cs-value" :style="{ color: creditColor(creditScore) }">{{ creditScore }}</div>
              <el-progress
                :percentage="creditScore"
                :color="creditColor(creditScore)"
                :stroke-width="10"
                :show-text="false"
                style="width:200px"
              />
              <div class="cs-level" :style="{ color: creditColor(creditScore) }">{{ creditLevel(creditScore) }}</div>
            </div>
            <div class="credit-stats-row">
              <div class="cs-item">
                <div class="csi-value danger">{{ summary.total_dangerous }}</div>
                <div class="csi-label">危险行为</div>
              </div>
              <div class="cs-item">
                <div class="csi-value">{{ summary.total_detections }}</div>
                <div class="csi-label">检测总数</div>
              </div>
              <div class="cs-item">
                <div class="csi-value phone">{{ summary.phone_count }}</div>
                <div class="csi-label">使用手机</div>
              </div>
              <div class="cs-item">
                <div class="csi-value yawn">{{ summary.yawn_count }}</div>
                <div class="csi-label">打哈欠</div>
              </div>
              <div class="cs-item">
                <div class="csi-value" style="color:#b37feb">{{ myRank }}</div>
                <div class="csi-label">排名 / {{ totalDrivers }}人</div>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16">
      <el-col :span="24">
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">驾驶行为分布（饼状图）</span></template>
          <div style="display:flex;align-items:center;justify-content:center;gap:40px;padding:20px">
            <canvas ref="pieCanvas" width="240" height="240"></canvas>
            <div class="pie-legend-col">
              <div v-for="item in pieItems" :key="item.key" class="pie-legend-row">
                <span class="pie-dot" :style="{ background: item.color }"></span>
                <span class="pie-lbl">{{ item.label }}</span>
                <span class="pie-val">{{ item.value }}次</span>
                <span class="pie-pct">{{ piePercent(item.value) }}%</span>
              </div>
            </div>
          </div>
          <div v-if="!hasData" style="text-align:center;color:#909399;padding:40px">暂无检测数据</div>
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="hover" style="margin-top:16px">
      <template #header>
        <span style="font-weight:500">最新检测状态</span>
        <el-tag v-if="latest" :type="latest.is_dangerous ? 'danger' : 'success'" style="margin-left:10px">
          {{ latest.is_dangerous ? '危险' : '正常' }}
        </el-tag>
      </template>
      <div v-if="latest" class="latest-detection">
        <div class="detection-item"><span class="label">检测类别：</span><span class="value">{{ categoryLabel(latest.category) }}</span></div>
        <div class="detection-item"><span class="label">检测时间：</span><span class="value">{{ formatTime(latest.detected_at) }}</span></div>
        <div class="detection-item"><span class="label">危险状态：</span><span class="value"><el-tag :type="latest.is_dangerous ? 'danger' : 'success'" size="small">{{ latest.is_dangerous ? '危险' : '正常' }}</el-tag></span></div>
      </div>
      <el-empty v-else description="暂无检测记录" />
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, nextTick } from 'vue'
import { detectionAPI, authAPI } from '../../api'

const pieCanvas = ref(null)
const latest = ref(null)
const creditScore = ref(100)
const myRank = ref('-')
const totalDrivers = ref(0)
const dailyStats = ref([])
const summary = ref({ total_dangerous: 0, total_detections: 0, yawn_count: 0, no_seatbelt_count: 0, phone_count: 0, steering_wheel_count: 0, hand_count: 0 })

const hasData = computed(() => summary.value.total_detections > 0)

const pieItems = computed(() => {
  const s = summary.value
  const items = [
    { key: 'phone', label: '使用手机', value: s.phone_count || 0, color: '#f56c6c' },
    { key: 'yawn', label: '打哈欠', value: s.yawn_count || 0, color: '#e6a23c' },
    { key: 'no_seatbelt', label: '未系安全带', value: s.no_seatbelt_count || 0, color: '#f89898' },
    { key: 'yawn', label: '打哈欠', value: s.yawn_count || 0, color: '#e6a23c' },
    { key: 'hand', label: '手部', value: s.hand_count || 0, color: '#67c23a' },
  ]
  return items.filter(i => i.value > 0)
})

function piePercent(value) {
  const total = pieItems.value.reduce((a, i) => a + i.value, 0)
  if (!total) return '0'
  return ((value / total) * 100).toFixed(1)
}

const categoryMap = { yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带', phone: '使用手机', hands_off: '双手离方向盘' }

function categoryLabel(cat) { return categoryMap[cat] || cat }
function formatTime(t) { return t ? new Date(t).toLocaleString('zh-CN') : '' }

function creditColor(score) {
  if (score >= 90) return '#52c41a'
  if (score >= 70) return '#1890ff'
  if (score >= 50) return '#fa8c16'
  if (score >= 30) return '#f5222d'
  return '#722ed1'
}
function creditLevel(score) {
  if (score >= 90) return '优秀'
  if (score >= 70) return '良好'
  if (score >= 50) return '一般'
  if (score >= 30) return '较差'
  return '危险'
}

function drawUserPie() {
  const canvas = pieCanvas.value
  if (!canvas) return
  const ctx = canvas.getContext('2d')
  const cx = 120, cy = 120, r = 100
  ctx.clearRect(0, 0, 240, 240)

  const data = pieItems.value
  if (data.length === 0) {
    ctx.fillStyle = 'rgba(255,255,255,0.1)'
    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill()
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

  ctx.beginPath()
  ctx.arc(cx, cy, 45, 0, Math.PI * 2)
  ctx.fillStyle = 'rgba(255,255,255,0.9)'
  ctx.fill()
  ctx.fillStyle = '#333'
  ctx.font = 'bold 16px sans-serif'
  ctx.textAlign = 'center'
  ctx.fillText(total, cx, cy + 6)
}

onMounted(async () => {
  try {
    const statsRes = await detectionAPI.getStats({ days: 7 })
    summary.value = statsRes.summary
    dailyStats.value = statsRes.daily_stats || []
    creditScore.value = statsRes.credit_score ?? 100

    // Fetch user ranking
    try {
      const rankRes = await authAPI.getMyRanking()
      myRank.value = rankRes.rank ?? '-'
      totalDrivers.value = rankRes.total ?? 0
    } catch (e) {}

    const realtimeRes = await detectionAPI.getRealtime()
    latest.value = realtimeRes.latest
    if (realtimeRes.credit_score != null) creditScore.value = realtimeRes.credit_score

    await nextTick()
    drawUserPie()
  } catch (e) {}
})
</script>

<style scoped>
.credit-banner { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
.credit-banner :deep(.el-card__body) { padding: 0; }
.credit-banner-inner { display: flex; align-items: center; padding: 20px 24px; gap: 40px; }
.credit-score-display { display: flex; flex-direction: column; align-items: center; gap: 4px; min-width: 140px; }
.cs-label { font-size: 12px; color: rgba(255,255,255,0.8); }
.cs-value { font-size: 48px; font-weight: bold; line-height: 1; color: #52c41a; }
.cs-level { font-size: 14px; font-weight: 500; }
.credit-stats-row { display: flex; gap: 32px; flex: 1; justify-content: center; }
.cs-item { text-align: center; }
.csi-value { font-size: 28px; font-weight: bold; color: #fff; }
.csi-value.danger { color: #ff7875; }
.csi-value.phone { color: #ffd666; }
.csi-value.yawn { color: #ff9c6e; }
.csi-label { font-size: 11px; color: rgba(255,255,255,0.7); margin-top: 2px; }
.stat-card { text-align: center; }
.pie-legend-col { display: flex; flex-direction: column; gap: 12px; }
.pie-legend-row { display: flex; align-items: center; gap: 10px; font-size: 14px; }
.pie-dot { width: 14px; height: 14px; border-radius: 3px; flex-shrink: 0; }
.pie-lbl { width: 80px; color: #606266; }
.pie-val { width: 50px; text-align: right; font-weight: 500; }
.pie-pct { width: 50px; text-align: right; color: #909399; }
.stat-value { font-size: 36px; font-weight: bold; color: #409eff; }
.stat-value.danger { color: #f56c6c; }
.stat-value.phone { color: #e6a23c; }
.stat-value.yawn { color: #f89898; }
.stat-label { font-size: 14px; color: #909399; margin-top: 4px; }
.latest-detection { display: flex; gap: 40px; padding: 10px 0; }
.detection-item .label { color: #909399; font-size: 14px; }
.detection-item .value { font-weight: 500; font-size: 14px; }
</style>