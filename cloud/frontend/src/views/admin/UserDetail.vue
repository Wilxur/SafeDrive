<template>
  <div v-loading="loading">
    <el-button text @click="$router.push('/admin/users')">
      <el-icon>
        <ArrowLeft />
      </el-icon>返回司机列表
    </el-button>

    <el-row :gutter="16" style="margin-top:12px">
      <!-- 左侧：用户信息 + 信用分 + 拉黑 -->
      <el-col :span="7">
        <!-- 用户信息卡片 -->
        <el-card shadow="hover">
          <template #header>
            <div class="header-row">
              <span style="font-weight:500">司机档案</span>
              <el-tag v-if="userDetail?.blacklisted" type="danger" size="small" effect="dark">已拉黑</el-tag>
              <el-tag v-else-if="userDetail?.is_active" type="success" size="small">正常</el-tag>
            </div>
          </template>
          <div v-if="userDetail" class="profile-grid">
            <div class="info-row"><span class="label">用户名</span><span>{{ userDetail.username }}</span></div>
            <div class="info-row"><span class="label">姓名</span><span>{{ userDetail.real_name || '-' }}</span></div>
            <div class="info-row"><span class="label">物流公司</span>
          <el-select v-model="editCompanyId" placeholder="选择公司" size="small" style="width:130px" clearable>
            <el-option v-for="c in companies" :key="c.id" :label="c.name" :value="c.id" />
          </el-select>
          <el-button size="small" type="primary" @click="handleUpdateUser" :loading="savingCompany" style="margin-left:6px">保存</el-button>
        </div>
                    <div class="info-row"><span class="label">车牌号</span>
          <el-input v-model="editPlate" size="small" style="width:130px" placeholder="输入车牌号" />
          <el-button size="small" type="primary" @click="handleUpdateUser" :loading="savingCompany" style="margin-left:6px">保存</el-button>
        </div>
            <div class="info-row"><span class="label">邮箱</span><span>{{ userDetail.email || '-' }}</span></div>
            <div class="info-row"><span class="label">设备ID</span><span>{{ userDetail.device_id || '-' }}</span></div>
          </div>
        </el-card>

        <!-- 信用分卡片 -->
        <el-card shadow="hover" style="margin-top:12px">
          <template #header><span style="font-weight:500">信用分管理</span></template>
          <div class="credit-card">
            <div class="big-score" :style="{ color: creditColor(userDetail?.credit_score) }">
              {{ userDetail?.credit_score || 0 }}
              <span class="big-score-label">/ 100</span>
            </div>
            <div class="credit-stats">
              <div class="cs-item"><span class="cs-lbl">违规次数</span><span class="cs-val danger">{{
                userDetail?.violation_count ||
                  0 }}</span></div>
              <div class="cs-item"><span class="cs-lbl">信用等级</span><span class="cs-val"
                  :style="{ color: creditColor(userDetail?.credit_score) }">{{ creditLevel(userDetail?.credit_score)
                  }}</span>
              </div>
            </div>

            <!-- 拉黑按钮 -->
            <div style="margin-top:12px;display:flex;gap:8px">
              <el-button :type="userDetail?.blacklisted ? 'success' : 'danger'" style="flex:1" :loading="blacklisting"
                @click="handleBlacklist">
                <el-icon>
                  <RemoveFilled />
                </el-icon>
                {{ userDetail?.blacklisted ? '移出黑名单' : '加入黑名单' }}
              </el-button>
            </div>

            <!-- 手动调整信用分 -->
            <div style="margin-top:12px;padding-top:12px;border-top:1px solid #f0f0f0">
              <div style="display:flex;gap:8px;align-items:center">
                <el-input-number v-model="creditAdjust" :min="-50" :max="50" size="small" style="width:120px" />
                <el-button size="small" type="primary" :loading="adjusting" @click="handleAdjustCredit">调整</el-button>
              </div>
              <el-input v-model="creditReason" size="small" placeholder="调整原因（选填）" style="margin-top:6px" />
            </div>
          </div>
        </el-card>

        <!-- 信用分变更记录 -->
        <el-card shadow="hover" style="margin-top:12px">
          <template #header><span style="font-weight:500">信用分变更记录</span></template>
          <div v-if="creditLogs.length" class="credit-log-list">
            <div v-for="log in creditLogs" :key="log.id" class="cl-item">
              <div class="cl-left">
                <div class="cl-change" :class="log.change > 0 ? 'up' : 'down'">
                  {{ log.change > 0 ? '+' : '' }}{{ log.change }}
                </div>
                <div class="cl-info">
                  <div class="cl-reason">{{ log.reason }}</div>
                  <div class="cl-time">{{ formatTime(log.created_at) }}</div>
                </div>
              </div>
              <div class="cl-after">{{ log.score_after }}分</div>
            </div>
          </div>
          <el-empty v-else description="暂无记录" :image-size="60" />
        </el-card>

        <!-- 发送预警 -->
        <el-card shadow="hover" style="margin-top:12px">
          <template #header><span style="font-weight:500">发送预警提醒</span></template>
          <el-form :model="alertForm" label-position="top">
            <el-form-item label="预警类型">
              <el-select v-model="alertForm.alert_type" style="width:100%">
                <el-option label="警告" value="warning" />
                <el-option label="提醒" value="reminder" />
                <el-option label="紧急" value="urgent" />
              </el-select>
            </el-form-item>
            <el-form-item label="标题">
              <el-input v-model="alertForm.title" placeholder="请输入预警标题" />
            </el-form-item>
            <el-form-item label="消息内容">
              <el-input v-model="alertForm.message" type="textarea" :rows="3" placeholder="请输入预警消息内容" />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="sending" @click="sendAlert" style="width:100%">发送预警</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-col>

      <!-- 右侧：检测数据 + 统计 -->
      <el-col :span="17">
        <!-- 近7天驾驶统计 -->
        <el-card shadow="hover">
          <template #header><span style="font-weight:500">今日驾驶统计</span></template>
          <div class="stats-grid">
            <div class="stat-box" v-for="s in statItems" :key="s.key">
              <div class="stat-num" :style="{ color: s.color }">{{ weekStats[s.key] || 0 }}</div>
              <div class="stat-name">{{ s.label }}</div>
            </div>
          </div>
        <el-empty description="暂无数据" />
</el-card>

            <!-- 历史趋势 -->
            <el-row :gutter="12" style="margin-top:12px">
              <el-col :span="12">
                <el-card shadow="hover">
                  <template #header>
                    <div class="header-row">
                      <span style="font-weight:500">每日趋势</span>
                      <div>
                        <el-button size="small" :type="historyDays === 7 ? 'primary' : ''"
                          @click="fetchHistory(7)">7天</el-button>
                        <el-button size="small" :type="historyDays === 30 ? 'primary' : ''"
                          @click="fetchHistory(30)">30天</el-button>
                      </div>
                    </div>
                  </template>
                  <div ref="historyChartRef" style="height:200px"></div>
                  <div v-if="historyStats.length === 0" style="text-align:center;color:#909399;padding:30px">暂无历史数据
                  </div>
                </el-card>
              </el-col>
              <el-col :span="12">
                <el-card shadow="hover">
                  <template #header><span style="font-weight:500">违规种类饼图</span></template>
                  <div class="pie-small-container">
                    <canvas ref="detailPieCanvas" width="160" height="160"></canvas>
                    <div class="pie-small-legend">
                      <div v-for="item in detailPieData" :key="item.key" class="legend-item-s">
                        <span class="legend-dot-s" :style="{ background: item.color }"></span>
                        <span>{{ item.label }}</span>
                        <span style="margin-left:auto;font-weight:500">{{ item.value }}</span>
                      </div>
                    </div>
                  </div>
                </el-card>
              </el-col>
            </el-row>

            <!-- 危险驾驶记录 -->
            <el-card shadow="hover" style="margin-top:12px">
              <template #header>
                <div class="header-row">
                  <span style="font-weight:500">危险驾驶记录</span>
                  <span style="font-size:12px;color:#909399">置信度已隐藏</span>
                </div>
              </template>
              <el-table :data="dangerousRecords" stripe size="small" max-height="320">
                <el-table-column label="类别" width="130">
                  <template #default="{ row }">
                    <el-tag :type="dangerType(row.category)" size="small">{{ categoryLabel(row.category) }}</el-tag>
                  </template>
                </el-table-column>
                <el-table-column prop="detected_at" label="检测时间" min-width="160">
                  <template #default="{ row }">{{ formatTime(row.detected_at) }}</template>
                </el-table-column>
                <el-table-column prop="device_id" label="设备" width="130" />
                <el-table-column prop="image_url" label="截图" width="100">
                  <template #default="{ row }">
                    <el-button v-if="row.image_url" text type="primary" size="small"
                      @click="previewImage(row.image_url)">查看</el-button>
                    <span v-else style="color:#bbb">-</span>
                  </template>
                </el-table-column>
              </el-table>
            </el-card>
      </el-col>
    </el-row>

    <!-- 图片预览 -->
    <el-image-viewer v-if="previewUrl" :url-list="[previewUrl]" @close="previewUrl = ''" />
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, nextTick } from 'vue'
import { useRoute } from 'vue-router'
import { adminAPI } from '../../api'
import { ElMessage } from 'element-plus'

const route = useRoute()
const userId = route.params.id

const loading = ref(false)
const sending = ref(false)
const blacklisting = ref(false)
const adjusting = ref(false)
const userDetail = ref(null)
const latestDetection = ref(null)
const todayStat = ref(null)
const unreadAlerts = ref(0)
const creditLogs = ref([])
const historyStats = ref([])
const weekStats = ref({})
const historyDays = ref(7)
const dangerousRecords = ref([])
const historyChartRef = ref(null)
const detailPieCanvas = ref(null)
const previewUrl = ref('')
const creditAdjust = ref(0)
const creditReason = ref('')

const statItems = [
  { key: 'no_seatbelt_count', label: '未系安全带', color: '#f89898' },
  { key: 'phone_count', label: '使用手机', color: '#f56c6c' },
  { key: 'yawn_count', label: '打哈欠', color: '#e6a23c' },
  { key: 'hand_count', label: '手部', color: '#67c23a' },
  { key: 'total_dangerous', label: '危险总数', color: '#f5222d' },
  { key: 'credit_deducted', label: '信用扣分', color: '#722ed1' },
]

const categoryMap = {
  yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带',
  phone: '使用手机', steering_wheel: '方向盘', hand: '手部',
}

const detailPieData = computed(() => {
  const s = weekStats.value || {}
  const items = [
    { key: 'no_seatbelt_count', label: '未系安全带', color: '#f89898', value: s.no_seatbelt_count || 0 },
    { key: 'phone_count', label: '使用手机', color: '#f56c6c', value: s.phone_count || 0 },
    { key: 'yawn_count', label: '打哈欠', color: '#e6a23c', value: s.yawn_count || 0 },
    { key: 'yawn_count', label: '打哈欠', color: '#e6a23c', value: s.yawn_count || 0 },
    { key: 'hand_count', label: '手部', color: '#67c23a', value: s.hand_count || 0 },
  ]
  return items.filter(i => i.value > 0)
})

const alertForm = reactive({
  alert_type: 'warning',
  title: '驾驶安全提醒',
  message: '',
})

function categoryLabel(cat) { return categoryMap[cat] || cat }

function dangerType(cat) {
  const m = { yawn: '', seatbelt: '', no_seatbelt: 'warning', phone: 'danger', hands_off: 'danger' }
  return m[cat] || 'info'
}

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

function previewImage(url) { previewUrl.value = url }

function drawDetailPie() {
  const canvas = detailPieCanvas.value
  if (!canvas) return
  const ctx = canvas.getContext('2d')
  const cx = 80, cy = 80, r = 70
  ctx.clearRect(0, 0, 160, 160)

  const data = detailPieData.value
  if (data.length === 0) {
    ctx.fillStyle = '#f0f0f0'
    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill()
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
  ctx.arc(cx, cy, 30, 0, Math.PI * 2)
  ctx.fillStyle = '#fff'
  ctx.fill()
  ctx.fillStyle = '#333'
  ctx.font = 'bold 14px sans-serif'
  ctx.textAlign = 'center'
  ctx.fillText(total, cx, cy + 5)
}

function renderHistoryChart() {
  const container = historyChartRef.value
  if (!container || historyStats.value.length === 0) return
  const safe = historyStats.value.map(s => ({ ...s, total_dangerous: s.total_dangerous || 0 }))
  const max = Math.max(...safe.map(s => s.total_dangerous), 1)
  let html = '<div style="display:flex;align-items:flex-end;height:150px;gap:6px;padding:20px 0">'
  safe.forEach(s => {
    const h = (s.total_dangerous / max) * 130
    html += '<div style="flex:1;display:flex;flex-direction:column;align-items:center;gap:3px">'
    html += `<span style="font-size:10px;color:#f56c6c">${s.total_dangerous}</span>`
    html += `<div style="width:100%;height:${Math.max(h, 2)}px;background:linear-gradient(180deg,#f56c6c,#f56c6c99);border-radius:3px"></div>`
    html += `<span style="font-size:9px;color:#909399">${s.stat_date ? s.stat_date.slice(5) : ''}</span>`
    html += '</div>'
  })
  html += '</div>'
  container.innerHTML = html
}

async function fetchHistory(days = 7) {
  historyDays.value = days
  try {
    const res = await adminAPI.getUserHistory(userId, { days })
    historyStats.value = res.daily_stats || []
    await nextTick()
    renderHistoryChart()
  } catch (e) { }
}

async function handleBlacklist() {
  blacklisting.value = true
  try {
    const action = userDetail.value.blacklisted ? 'unblacklist' : 'blacklist'
    const res = await adminAPI.toggleBlacklist(userId, action)
    ElMessage.success(res.message)
    userDetail.value.blacklisted = res.blacklisted
  } catch (e) { } finally {
    blacklisting.value = false
  }
}

async function handleAdjustCredit() {
  if (creditAdjust.value === 0) { ElMessage.warning('请调整信用分'); return }
  adjusting.value = true
  try {
    const res = await adminAPI.adjustCredit(userId, {
      change: creditAdjust.value,
      reason: creditReason.value || (creditAdjust.value > 0 ? '管理员加分' : '管理员扣分'),
    })
    ElMessage.success(`信用分已调整 (${creditAdjust.value > 0 ? '+' : ''}${creditAdjust.value})`)
    userDetail.value.credit_score = res.credit_score
    creditAdjust.value = 0
    creditReason.value = ''
    // 刷新信用日志
    const detail = await adminAPI.getUserDetail(userId)
    creditLogs.value = detail.credit_logs || []
  } catch (e) { } finally {
    adjusting.value = false
  }
}

async function sendAlert() {
  if (!alertForm.message) { ElMessage.warning('请输入预警消息内容'); return }
  sending.value = true
  try {
    await adminAPI.sendAlert({
      user_id: parseInt(userId),
      alert_type: alertForm.alert_type,
      title: alertForm.title,
      message: alertForm.message,
    })
    ElMessage.success('预警已发送')
    alertForm.message = ''
  } catch (e) { } finally {
    sending.value = false
  }
}

const companies = ref([])
const editCompanyId = ref(null)
const editPlate = ref('')
const savingCompany = ref(false)

async function loadCompanies() {
  try {
    const res = await adminAPI.getCompanies()
    companies.value = res.companies || []
  } catch (e) {}
}

async function handleUpdateUser() {
  savingCompany.value = true
  try {
    const data = {}
    if (editCompanyId.value !== null && editCompanyId.value !== undefined) data.company_id = editCompanyId.value || null
    if (editPlate.value !== undefined) data.license_plate = editPlate.value
    await adminAPI.updateUser(userId, data)
    ElMessage.success('保存成功')
    // Refresh user detail
    const res = await adminAPI.getUserDetail(userId)
    userDetail.value = res.user
  } catch (e) {}
  finally { savingCompany.value = false }
}

onMounted(async () => {
  loading.value = true
  try {
    await loadCompanies()
    const res = await adminAPI.getUserDetail(userId)
    userDetail.value = res.user
    editCompanyId.value = res.user?.company_id ?? null
    editPlate.value = res.user?.license_plate || ''
    latestDetection.value = res.latest_detection
    unreadAlerts.value = res.unread_alerts
    creditLogs.value = res.credit_logs || []

    // 获取7天统计数据用于饼图
    const histRes = await adminAPI.getUserHistory(userId, { days: 7 })
    const stats = histRes.daily_stats || []
    historyStats.value = stats
    weekStats.value = {
      no_seatbelt_count: stats.reduce((a, s) => a + (s.no_seatbelt_count || 0), 0),
      phone_count: stats.reduce((a, s) => a + (s.phone_count || 0), 0),
      yawn_count: stats.reduce((a, s) => a + (s.yawn_count || 0), 0),
      yawn_count: stats.reduce((a, s) => a + (s.yawn_count || 0), 0),
      hand_count: stats.reduce((a, s) => a + (s.hand_count || 0), 0),
      total_dangerous: stats.reduce((a, s) => a + (s.total_dangerous || 0), 0),
      total_detections: stats.reduce((a, s) => a + (s.total_detections || 0), 0),
      credit_deducted: stats.reduce((a, s) => a + (s.credit_deducted || 0), 0),
    }

    const recordsRes = await adminAPI.getUserRecords(userId, { dangerous_only: 1, per_page: 20 })
    dangerousRecords.value = recordsRes.records || []

    await nextTick()
    drawDetailPie()
    renderDailyChart()
  } catch (e) { } finally {
    loading.value = false
  }
})
</script>

<style scoped>
.header-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

/* 左侧档案 */
.profile-grid {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.info-row {
  display: flex;
  align-items: center;
}

.info-row .label {
  color: #909399;
  width: 70px;
  font-size: 13px;
  flex-shrink: 0;
}

.info-row span:last-child {
  font-size: 13px;
  color: #333;
}

/* 信用分卡片 */
.credit-card {
  text-align: center;
}

.big-score {
  font-size: 52px;
  font-weight: bold;
  line-height: 1;
}

.big-score-label {
  font-size: 16px;
  color: #909399;
}

.credit-stats {
  display: flex;
  justify-content: center;
  gap: 24px;
  margin-top: 8px;
}

.cs-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
}

.cs-lbl {
  font-size: 11px;
  color: #909399;
}

.cs-val {
  font-size: 18px;
  font-weight: 600;
}

.cs-val.danger {
  color: #f56c6c;
}

/* 信用日志 */
.credit-log-list {
  max-height: 260px;
  overflow-y: auto;
}

.cl-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #f5f5f5;
}

.cl-item:last-child {
  border-bottom: none;
}

.cl-left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.cl-change {
  width: 44px;
  height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 6px;
  font-size: 14px;
  font-weight: 600;
  flex-shrink: 0;
}

.cl-change.up {
  background: #f6ffed;
  color: #52c41a;
}

.cl-change.down {
  background: #fff1f0;
  color: #f5222d;
}

.cl-info {
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.cl-reason {
  font-size: 12px;
  color: #333;
}

.cl-time {
  font-size: 10px;
  color: #bbb;
}

.cl-after {
  font-size: 13px;
  font-weight: 600;
  color: #333;
}

/* 统计网格 */
.stats-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 12px;
}

.stat-box {
  text-align: center;
  padding: 12px 0;
}

.stat-num {
  font-size: 24px;
  font-weight: bold;
}

.stat-name {
  font-size: 11px;
  color: #909399;
  margin-top: 2px;
}

/* 饼图 */
.pie-small-container {
  display: flex;
  gap: 16px;
  align-items: center;
}

.pie-small-legend {
  display: flex;
  flex-direction: column;
  gap: 4px;
  flex: 1;
}

.legend-item-s {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
}

.legend-dot-s {
  width: 8px;
  height: 8px;
  border-radius: 2px;
  flex-shrink: 0;
}
</style>