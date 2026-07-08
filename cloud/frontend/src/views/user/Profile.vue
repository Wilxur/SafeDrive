<template>
  <el-row :gutter="16">
    <el-col :span="14">
      <el-card shadow="hover">
        <template #header><span style="font-weight:500">个人信息</span></template>
        <el-form :model="form" label-width="100px" v-loading="loading">
          <el-form-item label="用户名"><el-input v-model="form.username" disabled /></el-form-item>
          <el-form-item label="角色">
            <el-tag :type="form.role === 'admin' ? 'danger' : 'primary'">{{ form.role === 'admin' ? '管理员' : '司机' }}</el-tag>
          </el-form-item>
          <el-form-item label="真实姓名"><el-input v-model="form.real_name" placeholder="请输入真实姓名" /></el-form-item>
          <el-form-item label="邮箱"><el-input v-model="form.email" placeholder="请输入邮箱" /></el-form-item>
          <el-form-item label="车牌号"><el-input v-model="form.license_plate" placeholder="请输入车牌号" /></el-form-item>
          <el-form-item label="设备ID"><el-input v-model="form.device_id" placeholder="关联的设备ID" /></el-form-item>
          <el-form-item>
            <el-button type="primary" @click="handleUpdate">保存修改</el-button>
          </el-form-item>
        </el-form>
      </el-card>
    </el-col>

    <el-col :span="10">
      <el-card shadow="hover">
        <template #header><span style="font-weight:500">信用概况</span></template>
        <div class="credit-card">
          <div class="big-score" :style="{ color: creditColor(form.credit_score) }">{{ form.credit_score }}<span class="big-unit">/100</span></div>
          <div class="credit-tag" :style="{ background: creditColor(form.credit_score) + '20', color: creditColor(form.credit_score), border: '1px solid ' + creditColor(form.credit_score) + '40' }">
            {{ creditLevel(form.credit_score) }}
          </div>
          <div class="credit-detail-row">
            <div class="cd-item"><span class="cd-label">所属公司</span><span class="cd-value">{{ form.company_name || '未分配' }}</span></div>
            <div class="cd-item"><span class="cd-label">车牌号</span><span class="cd-value">{{ form.license_plate || '未设置' }}</span></div>
          </div>
        </div>
      </el-card>
    </el-col>
  </el-row>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { authAPI } from '../../api'
import { useAuthStore } from '../../store/auth'
import { ElMessage } from 'element-plus'

const auth = useAuthStore()
const loading = ref(false)
const form = reactive({ username: '', role: '', real_name: '', email: '', device_id: '', license_plate: '', company_name: '', credit_score: 100 })

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

onMounted(async () => {
  try { await auth.fetchProfile() } catch (e) {}
  if (auth.user) Object.assign(form, auth.user)
})

async function handleUpdate() {
  loading.value = true
  try {
    await authAPI.updateProfile({
      real_name: form.real_name,
      email: form.email,
      device_id: form.device_id,
      license_plate: form.license_plate,
    })
    ElMessage.success('保存成功')
    await auth.fetchProfile()
    if (auth.user) Object.assign(form, auth.user)
  } catch (e) {} finally {
    loading.value = false
  }
}
</script>

<style scoped>
.credit-card { text-align: center; padding: 20px 0; }
.big-score { font-size: 56px; font-weight: bold; line-height: 1; }
.big-unit { font-size: 18px; color: #909399; margin-left: 4px; }
.credit-tag { display: inline-block; padding: 4px 20px; border-radius: 20px; font-size: 14px; font-weight: 500; margin-top: 8px; }
.credit-detail-row { margin-top: 24px; display: flex; flex-direction: column; gap: 12px; text-align: left; padding: 0 20px; }
.cd-item { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #f5f5f5; }
.cd-label { color: #909399; font-size: 13px; }
.cd-value { font-weight: 500; color: #333; font-size: 13px; }
</style>