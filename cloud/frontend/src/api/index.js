import axios from 'axios'
import { ElMessage } from 'element-plus'
import router from '../router'

const api = axios.create({
  baseURL: '/api',
  timeout: 10000,
})

// 请求拦截器：自动添加 token
api.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('token')
    if (token) {
      config.headers.Authorization = `Bearer ${token}`
    }
    return config
  },
  (error) => Promise.reject(error)
)

// 响应拦截器：统一处理错误
api.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.response) {
      const { status, data } = error.response
      if (status === 401) {
        localStorage.removeItem('token')
        localStorage.removeItem('user')
        router.push('/login')
        ElMessage.error('登录已过期，请重新登录')
      } else if (status === 403) {
        ElMessage.error(data?.error || '权限不足')
      } else {
        ElMessage.error(data?.error || '请求失败')
      }
    } else {
      ElMessage.error('网络错误，请检查连接')
    }
    return Promise.reject(error)
  }
)

export default api

// ==================== 认证相关 ====================
export const authAPI = {
  login(data) { return api.post('/auth/login', data) },
  register(data) { return api.post('/auth/register', data) },
  getProfile() { return api.get('/auth/profile') },
  updateProfile(data) { return api.put('/auth/profile', data) },
  // 信用规则（司机可访问）
  getCreditRules() { return api.get('/auth/credit/rules') },
  // 用户排名
  getMyRanking() { return api.get('/auth/ranking') },
  // 信用分变更记录
  getCreditHistory(params) { return api.get('/auth/credit/history', { params }) },
}

// ==================== 检测数据相关 ====================
export const detectionAPI = {
  upload(data) { return api.post('/detection/upload', data) },
  getRecords(params) { return api.get('/detection/records', { params }) },
  getStats(params) { return api.get('/detection/stats', { params }) },
  getRealtime() { return api.get('/detection/realtime') },
  // 今日检测汇总
  getToday() { return api.get('/detection/today') },
}

// ==================== 预警相关 ====================
export const alertsAPI = {
  getMyAlerts(params) { return api.get('/alerts/my', { params }) },
  markAsRead(id) { return api.put(`/alerts/${id}/read`) },
  markAllAsRead() { return api.put('/alerts/read-all') },
}

// ==================== 管理员相关 ====================
export const adminAPI = {
  // 用户
  getUsers() { return api.get('/admin/users') },
  getUserDetail(id) { return api.get(`/admin/users/${id}`) },
  getUserHistory(id, params) { return api.get(`/admin/users/${id}/history`, { params }) },
  getUserRecords(id, params) { return api.get(`/admin/users/${id}/records`, { params }) },
  // 更新用户（分配公司、车牌等）
  updateUser(id, data) { return api.put(`/admin/users/${id}`, data) },
  // 黑名单
  toggleBlacklist(id, action) { return api.post(`/admin/users/${id}/blacklist`, { action }) },
  // 信用分
  adjustCredit(id, data) { return api.post(`/admin/users/${id}/credit`, data) },
  // 预警
  getAlerts(params) { return api.get('/admin/alerts', { params }) },
  sendAlert(data) { return api.post('/admin/send-alert', data) },
  // 仪表盘
  getDashboard() { return api.get('/admin/dashboard') },
  // 用户信用排行
  getUserRanking() { return api.get('/admin/users/ranking') },
  // 公司管理
  getCompanies() { return api.get('/admin/companies') },
  createCompany(data) { return api.post('/admin/companies', data) },
  updateCompany(id, data) { return api.put(`/admin/companies/${id}`, data) },
  deleteCompany(id) { return api.delete(`/admin/companies/${id}`) },
}