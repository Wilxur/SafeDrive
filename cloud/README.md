# 疲劳驾驶检测系统

## 项目简介

疲劳驾驶检测系统，支持管理员查看用户的危险驾驶行为并发送预警提醒，用户可以收到通知并做出调整。

**数据集类别：** no_seatbelt, phone, seatbelt, yawn, hand, steering_wheel, closed_eyes, open_eyes

## 技术栈

- **后端：** Python Flask + Flask-SQLAlchemy + Flask-JWT-Extended
- **前端：** Vue 3 + Vite + Element Plus + Pinia
- **数据库：** MySQL 8.0+
- **认证：** JWT Token

## 项目结构

```
├── backend/                # 后端服务
│   ├── app/
│   │   ├── __init__.py     # Flask 应用工厂
│   │   ├── models.py       # 数据库模型
│   │   └── routes/
│   │       ├── auth.py     # 认证接口
│   │       ├── detection.py# 检测数据接口
│   │       ├── alerts.py   # 预警接口
│   │       └── admin.py    # 管理员接口
│   ├── config.py           # 配置文件
│   ├── run.py              # 启动入口
│   ├── init_db.py          # 数据库初始化
│   └── requirements.txt    # Python 依赖
├── frontend/               # 前端项目
│   ├── src/
│   │   ├── api/index.js    # API 封装
│   │   ├── router/index.js # 路由配置
│   │   ├── store/auth.js   # 状态管理
│   │   ├── views/
│   │   │   ├── auth/       # 登录/注册
│   │   │   ├── user/       # 用户面板
│   │   │   └── admin/      # 管理员面板
│   │   └── App.vue
│   └── package.json
├── database/
│   └── schema.sql          # 数据库建表脚本
└── README.md
```

## 快速开始

### 1. 后端

```bash
# 安装 Python 依赖
cd backend
pip install -r requirements.txt

# 配置数据库（修改 .env 中的数据库连接信息）
# 先创建 MySQL 数据库：
mysql -u root -p < ../database/schema.sql

# 初始化表和默认账户
python init_db.py

# 启动后端服务
python run.py
# 服务运行在 http://localhost:5000
```

### 2. 前端

```bash
cd frontend
npm install
npm run dev
# 访问 http://localhost:5173
```

## 默认账户

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 管理员 | admin | admin123 |
| 测试用户 | test | test123 |

## API 接口概览

### 认证 `/api/auth`
- `POST /login` - 登录
- `POST /register` - 注册
- `GET /profile` - 获取个人信息
- `PUT /profile` - 更新个人信息

### 检测数据 `/api/detection`
- `POST /upload` - 上传检测数据
- `GET /records` - 获取检测记录
- `GET /stats` - 获取统计数据
- `GET /realtime` - 获取最新检测

### 预警 `/api/alerts`
- `GET /my` - 获取我的预警
- `PUT /:id/read` - 标记已读
- `PUT /read-all` - 全部已读

### 管理员 `/api/admin`
- `GET /users` - 用户列表
- `GET /users/:id` - 用户详情
- `GET /users/:id/history` - 用户历史
- `GET /users/:id/records` - 用户检测记录
- `GET /alerts` - 预警记录
- `POST /send-alert` - 发送预警
- `GET /dashboard` - 仪表盘统计

## 开发板上报数据格式

```json
POST /api/detection/upload
{
  "category": "phone",
  "confidence": 0.95,
  "device_id": "DEVICE-001",
  "image_url": "http://...",
  "detected_at": "2026-05-20T10:30:00"
}
```
