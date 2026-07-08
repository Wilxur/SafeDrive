<template>
  <div>
    <el-card shadow="hover">
      <template #header>
        <div class="header-row">
          <span style="font-weight:500">司机列表</span>
          <el-button type="primary" size="small" @click="showCompanyDialog = true">物流公司管理</el-button>
        </div>
      </template>

      <el-table :data="users" stripe v-loading="loading" @row-click="goToDetail" style="cursor:pointer">
        <el-table-column prop="id" label="ID" width="60" />
        <el-table-column prop="username" label="用户名" width="100" />
        <el-table-column prop="real_name" label="姓名" width="90" />
        <el-table-column label="公司" width="100">
          <template #default="{ row }">
            <el-tag size="small" type="info" v-if="row.company_name">{{ row.company_name }}</el-tag>
            <span v-else style="color:#bbb">-</span>
          </template>
        </el-table-column>
        <el-table-column prop="license_plate" label="车牌号" width="100" />
        <el-table-column label="信用分" width="100">
          <template #default="{ row }">
            <div class="credit-cell">
              <el-progress
                :percentage="row.credit_score"
                :color="creditColor(row.credit_score)"
                :stroke-width="14"
                :format="() => row.credit_score + '分'"
              />
            </div>
          </template>
        </el-table-column>
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag v-if="row.blacklisted" type="danger" size="small">已拉黑</el-tag>
            <el-tag v-else-if="!row.is_active" type="info" size="small">已禁用</el-tag>
            <el-tag v-else type="success" size="small">正常</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="violation_count" label="违规" width="60" align="center" />
        <el-table-column prop="created_at" label="注册时间" width="160">
          <template #default="{ row }">{{ formatTime(row.created_at) }}</template>
        </el-table-column>
        <el-table-column label="操作" width="140" fixed="right">
          <template #default="{ row }">
            <el-button type="primary" size="small" @click.stop="goToDetail(row)">详情</el-button>
            <el-button
              :type="row.blacklisted ? 'success' : 'danger'"
              size="small"
              @click.stop="handleBlacklist(row)"
            >{{ row.blacklisted ? '解封' : '拉黑' }}</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <!-- 公司管理对话框 -->
    <el-dialog v-model="showCompanyDialog" title="物流公司管理" width="600px">
      <div style="margin-bottom:12px">
        <el-button type="primary" size="small" @click="showAddCompany = true">新增公司</el-button>
      </div>
      <el-table :data="companies" stripe size="small">
        <el-table-column prop="name" label="公司名称" min-width="140" />
        <el-table-column prop="contact_person" label="联系人" width="100" />
        <el-table-column prop="phone" label="电话" width="120" />
        <el-table-column label="司机数" width="80" align="center">
          <template #default="{ row }">{{ row.driver_count || 0 }}</template>
        </el-table-column>
        <el-table-column label="操作" width="80">
          <template #default="{ row }">
            <el-popconfirm title="确定删除该公司？" @confirm="handleDeleteCompany(row.id)">
              <template #reference>
                <el-button text type="danger" size="small">删除</el-button>
              </template>
            </el-popconfirm>
          </template>
        </el-table-column>
      </el-table>

      <template #footer>
        <el-button @click="showCompanyDialog = false">关闭</el-button>
      </template>
    </el-dialog>

    <!-- 新增公司对话框 -->
    <el-dialog v-model="showAddCompany" title="新增物流公司" width="400px">
      <el-form :model="companyForm" label-width="80px">
        <el-form-item label="公司名称">
          <el-input v-model="companyForm.name" placeholder="请输入" />
        </el-form-item>
        <el-form-item label="联系人">
          <el-input v-model="companyForm.contact_person" placeholder="请输入" />
        </el-form-item>
        <el-form-item label="电话">
          <el-input v-model="companyForm.phone" placeholder="请输入" />
        </el-form-item>
        <el-form-item label="地址">
          <el-input v-model="companyForm.address" placeholder="请输入" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showAddCompany = false">取消</el-button>
        <el-button type="primary" @click="handleCreateCompany">确定</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { adminAPI } from '../../api'
import { ElMessage, ElMessageBox } from 'element-plus'

const router = useRouter()
const users = ref([])
const companies = ref([])
const loading = ref(false)
const showCompanyDialog = ref(false)
const showAddCompany = ref(false)
const companyForm = ref({ name: '', contact_person: '', phone: '', address: '' })

function formatTime(t) { return t ? new Date(t).toLocaleString('zh-CN') : '' }

function creditColor(score) {
  if (score >= 90) return '#52c41a'
  if (score >= 70) return '#1890ff'
  if (score >= 50) return '#fa8c16'
  if (score >= 30) return '#f5222d'
  return '#722ed1'
}

function goToDetail(row) { router.push(`/admin/users/${row.id}`) }

async function handleBlacklist(row) {
  try {
    const action = row.blacklisted ? 'unblacklist' : 'blacklist'
    const res = await adminAPI.toggleBlacklist(row.id, action)
    ElMessage.success(res.message)
    row.blacklisted = res.blacklisted
  } catch (e) {}
}

async function fetchCompanies() {
  try {
    const res = await adminAPI.getCompanies()
    companies.value = res.companies || []
  } catch (e) {}
}

async function handleCreateCompany() {
  if (!companyForm.value.name) { ElMessage.warning('请输入公司名称'); return }
  try {
    await adminAPI.createCompany(companyForm.value)
    ElMessage.success('公司已创建')
    showAddCompany.value = false
    companyForm.value = { name: '', contact_person: '', phone: '', address: '' }
    await fetchCompanies()
  } catch (e) {}
}

async function handleDeleteCompany(id) {
  try {
    await adminAPI.deleteCompany(id)
    ElMessage.success('已删除')
    await fetchCompanies()
  } catch (e) {}
}

onMounted(async () => {
  loading.value = true
  try {
    const res = await adminAPI.getUsers()
    users.value = res.users
  } catch (e) {} finally {
    loading.value = false
  }
})
</script>

<style scoped>
.header-row { display: flex; justify-content: space-between; align-items: center; }
.credit-cell { padding: 2px 0; }
.credit-cell :deep(.el-progress-bar) { margin-right: 0 !important; }
.credit-cell :deep(.el-progress__text) { display: none; }
</style>