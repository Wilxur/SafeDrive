<template>
  <div>
    <!-- 筛选栏 -->
    <el-card shadow="hover" style="margin-bottom: 20px">
      <el-form :inline="true" :model="filters">
        <el-form-item label="类别">
          <el-select v-model="filters.category" placeholder="全部类别" clearable style="width: 140px">
            <el-option v-for="(label, key) in categoryMap" :key="key" :label="label" :value="key" />
          </el-select>
        </el-form-item>
        <el-form-item label="日期">
          <el-date-picker v-model="filters.dateRange" type="daterange" range-separator="至" start-placeholder="开始日期" end-placeholder="结束日期" style="width: 240px" />
        </el-form-item>
        <el-form-item>
          <el-checkbox v-model="filters.dangerousOnly">仅危险行为</el-checkbox>
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="fetchRecords">查询</el-button>
          <el-button @click="resetFilters">重置</el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <!-- 记录列表 -->
    <el-card shadow="hover">
      <el-table :data="records" stripe style="width: 100%" v-loading="loading">
        <el-table-column prop="id" label="ID" width="70" />
        <el-table-column label="检测类别" width="140">
          <template #default="{ row }">
            <el-tag :type="row.is_dangerous ? 'danger' : 'success'" size="small">
              {{ categoryLabel(row.category) }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="是否危险" width="100">
          <template #default="{ row }">
            <el-tag :type="row.is_dangerous ? 'danger' : 'info'" size="small">
              {{ row.is_dangerous ? '是' : '否' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="detected_at" label="检测时间" min-width="180">
          <template #default="{ row }">
            {{ formatTime(row.detected_at) }}
          </template>
        </el-table-column>
        <el-table-column prop="device_id" label="设备ID" width="150" />
      </el-table>

      <div class="pagination-wrapper">
        <el-pagination
          v-model:current-page="page"
          :page-size="perPage"
          :total="total"
          layout="prev, pager, next, total"
          @current-change="fetchRecords"
        />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { detectionAPI } from '../../api'

const records = ref([])
const total = ref(0)
const page = ref(1)
const perPage = ref(20)
const loading = ref(false)

const categoryMap = {
  yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带',
  yawn: '打哈欠', seatbelt: '系安全带', no_seatbelt: '未系安全带', phone: '使用手机', hands_off: '双手离方向盘',
}

const filters = reactive({
  category: '',
  dateRange: null,
  dangerousOnly: false,
})

function categoryLabel(cat) {
  return categoryMap[cat] || cat
}

function formatTime(t) {
  if (!t) return ''
  return new Date(t).toLocaleString('zh-CN')
}

function resetFilters() {
  filters.category = ''
  filters.dateRange = null
  filters.dangerousOnly = false
  page.value = 1
  fetchRecords()
}

async function fetchRecords() {
  loading.value = true
  try {
    const params = { page: page.value, per_page: perPage.value }
    if (filters.category) params.category = filters.category
    if (filters.dateRange) {
      params.start_date = filters.dateRange[0].toISOString().slice(0, 10)
      params.end_date = filters.dateRange[1].toISOString().slice(0, 10)
    }
    if (filters.dangerousOnly) params.dangerous_only = 1

    const res = await detectionAPI.getRecords(params)
    records.value = res.records
    total.value = res.total
  } catch (e) {
    // ignore
  } finally {
    loading.value = false
  }
}

onMounted(fetchRecords)
</script>

<style scoped>
.pagination-wrapper {
  margin-top: 20px;
  display: flex;
  justify-content: flex-end;
}
</style>
