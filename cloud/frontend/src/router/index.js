import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '../store/auth'
import Login from '../views/auth/Login.vue'
import Register from '../views/auth/Register.vue'

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: Login,
    meta: { requiresAuth: false },
  },
  {
    path: '/register',
    name: 'Register',
    component: Register,
    meta: { requiresAuth: false },
  },
  {
    path: '/',
    component: () => import('../views/Layout.vue'),
    meta: { requiresAuth: true },
    redirect: '/dashboard',
    children: [
      {
        path: 'dashboard',
        name: 'UserDashboard',
        component: () => import('../views/user/Dashboard.vue'),
        meta: { title: '我的驾驶状态', role: 'user' },
      },
      {
        path: 'records',
        name: 'UserRecords',
        component: () => import('../views/user/Records.vue'),
        meta: { title: '检测记录', role: 'user' },
      },
      {
        path: 'alerts',
        name: 'UserAlerts',
        component: () => import('../views/user/Alerts.vue'),
        meta: { title: '预警提醒', role: 'user' },
      },
      {
        path: 'rules',
        name: 'UserRules',
        component: () => import('../views/user/Rules.vue'),
        meta: { title: '信用规则', role: 'user' },
      },
      {
        path: 'profile',
        name: 'UserProfile',
        component: () => import('../views/user/Profile.vue'),
        meta: { title: '个人信息', role: 'user' },
      },
      // Admin routes
      {
        path: 'admin',
        name: 'AdminDashboard',
        component: () => import('../views/admin/Dashboard.vue'),
        meta: { title: '管理面板', role: 'admin' },
      },
      {
        path: 'admin/users',
        name: 'AdminUsers',
        component: () => import('../views/admin/Users.vue'),
        meta: { title: '用户管理', role: 'admin' },
      },
      {
        path: 'admin/users/:id',
        name: 'AdminUserDetail',
        component: () => import('../views/admin/UserDetail.vue'),
        meta: { title: '用户详情', role: 'admin' },
      },
      {
        path: 'admin/alerts',
        name: 'AdminAlerts',
        component: () => import('../views/admin/Alerts.vue'),
        meta: { title: '预警管理', role: 'admin' },
      },
    ],
  },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

router.beforeEach((to, from, next) => {
  const auth = useAuthStore()

  if (to.meta.requiresAuth !== false && !auth.isLoggedIn) {
    return next('/login')
  }

  if (to.meta.role === 'admin' && !auth.isAdmin) {
    return next('/dashboard')
  }

  if (to.path === '/login' && auth.isLoggedIn) {
    return next(auth.isAdmin ? '/admin' : '/dashboard')
  }

  next()
})

export default router
