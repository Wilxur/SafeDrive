# SafeDrive — AI 驾驶员监控系统 (DMS)

> 基于 Infineon PSOC Edge E84 双核 MCU + 云端后端的端侧 AI 驾驶员行为检测系统

[![Platform](https://img.shields.io/badge/platform-PSOC%20Edge%20E84-blue)](https://www.infineon.com/KIT_PSE84_EVAL)
[![MCU](https://img.shields.io/badge/MCU-CM33%20%2B%20CM55-orange)](#)
[![AI](https://img.shields.io/badge/AI-YOLOv5%20%2B%20Ethos--U55-green)](#)
[![Cloud](https://img.shields.io/badge/cloud-Flask%20%2B%20Vue.js-purple)](#)

---

## 项目简介

SafeDrive 是一套完整的**驾驶员监控系统 (Driver Monitoring System)**，通过车内摄像头实时检测驾驶员危险行为，并在本地完成 AI 推理、LCD 可视化、语音报警，同时通过 Wi-Fi 将检测数据上传云端后端进行统计分析和信用评分。

系统分为两部分：
- **嵌入式端**：运行在 Infineon PSOC Edge E84 双核 MCU 上，负责视频采集、AI 推理、语音播报
- **云端**：Flask + Vue.js 全栈 Web 应用，负责数据存储、统计分析、用户管理

---

## 系统架构

```
+--------------------------------------------------------------+
|                     PSOC Edge E84 (嵌入式端)                    |
|                                                               |
|  +--------------+   共享内存    +--------------+               |
|  |   CM55 核心    |<---------->|   CM33 核心    |               |
|  |   (500MHz)    |  0x26480000  |   (250MHz)    |               |
|  |               |              |               |               |
|  | - USB摄像头    |              | - Wi-Fi STA   |               |
|  | - YOLOv5 推理  |              | - JWT 认证    |               |
|  | - NMS 后处理   |              | - HTTP 上报   |               |
|  | - LCD 渲染    |              |               |               |
|  | - I2S 语音    |              |               |               |
|  | - 帧差唤醒    |              |               |               |
|  +-------+-------+              +-------+-------+               |
|          |                               |                      |
|          v                               v                      |
|  Waveshare 4.3" LCD                   Flask 云服务器            |
|  TLV320DAC3100 扬声器                 POST /api/detection/upload|
+--------------------------------------------------------------+
```

---

## 核心特性

### 嵌入式端

| 特性 | 说明 |
|------|------|
| **双核异构** | CM55(500MHz+NPU)跑AI / CM33(250MHz)跑网络，256KB共享内存通信 |
| **YOLOv5 推理** | 6类目标检测，Ethos-U55 NPU 硬件加速，~10-20ms/帧 |
| **帧差唤醒** | 40x40灰度网格运动检测，静态场景跳过推理，大幅省电 |
| **I2S 语音报警** | TLV320DAC3100 DAC 驱动板载扬声器，5段中文语音提示 |
| **GPU 渲染** | VGLite GPU 做色彩转换+缩放，LCD 实时显示检测框和状态 |
| **Wi-Fi 联网** | JWT 认证 + HTTP 上报，支持断线重连和速率限制 |

### 云端

| 特性 | 说明 |
|------|------|
| **REST API** | Flask 后端，JWT 认证，完整 CRUD |
| **实时仪表盘** | Vue.js 前端，饼图统计 + 检测记录列表 |
| **信用评分** | 检测到危险行为自动扣分，支持黑名单机制 |
| **用户管理** | 注册/登录/设备绑定/管理员面板 |
| **一键部署** | deploy.sh 脚本，支持 Nginx + systemd |

---

## 目录结构

```
SafeDrive/
├── README.md                     # 项目总览
├── embedded/                     # 嵌入式端代码
│   ├── proj_cm33_s/              # CM33 Secure (安全启动)
│   ├── proj_cm33_ns/             # CM33 Non-Secure (网络通信)
│   │   ├── main.c                # 入口 + 启动CM55
│   │   ├── tcp_client.c          # Wi-Fi/JWT/HTTP
│   │   └── dms_config.h          # 网络凭证配置
│   ├── proj_cm55/                # CM55 (AI 推理核心)
│   │   ├── source/
│   │   │   ├── main.c            # 入口 + 创建RTOS任务
│   │   │   ├── inference_task.c  # YOLO推理 + NMS + 帧差唤醒
│   │   │   ├── lcd_task.c        # VGLite GPU渲染 + 显示
│   │   │   ├── usb_camera_task.c # USB UVC 摄像头驱动
│   │   │   ├── voice_player.c    # I2S 语音播报
│   │   │   └── voice_*.h         # 5段预录制语音PCM
│   │   └── model/                # DEEPCRAFT YOLOv5模型
│   ├── server/
│   │   └── server.py             # 嵌入式端 Flask 报警服务
│   └── shared_dms.h              # 双核共享内存协议
├── cloud/                        # 云端代码
│   ├── backend/                  # Flask 后端
│   │   ├── app/
│   │   │   ├── models.py         # 数据模型
│   │   │   └── routes/           # API 路由
│   │   │       ├── auth.py       # 认证
│   │   │       ├── detection.py  # 检测上报
│   │   │       ├── admin.py      # 管理面板
│   │   │       └── alerts.py     # 报警管理
│   │   ├── config.py             # 配置文件
│   │   └── run.py                # 启动入口
│   ├── frontend/                 # Vue.js 前端
│   │   └── src/views/           # 页面组件
│   ├── database/
│   │   └── schema.sql            # 数据库建表
│   └── deploy/                   # 部署脚本
└── docs/                         # 文档
    └── SafeDrive_DMS_项目介绍.md
```

---

## 硬件要求

| 组件 | 型号 | 备注 |
|------|------|------|
| 开发板 | PSOC Edge E84 Evaluation Kit | `KIT_PSE84_EVAL_EPC2` |
| 摄像头 | Logitech C920 / HBVCAM | USB UVC |
| 显示屏 | Waveshare 4.3" DSI LCD | 800x480 |
| 扬声器 | 板载 (TLV320DAC3100) | I2S 音频 DAC |
| Wi-Fi | 板载 SDIO | STA 模式 |

---

## 快速开始

### 嵌入式端

1. **环境准备**：安装 [ModusToolbox 3.7](https://www.infineon.com/modustoolbox) + Machine Learning Pack

2. **获取依赖**：
   ```bash
   cd embedded/proj_cm55
   make getlibs
   ```

3. **配置 Wi-Fi 和服务器地址**：编辑 `embedded/proj_cm33_ns/dms_config.h`
   ```c
   #define DMS_WIFI_SSID       "你的WiFi名"
   #define DMS_WIFI_PASSWORD   "你的WiFi密码"
   #define DMS_SERVER_HOST_STR "你的服务器IP"
   ```

4. **编译烧录**：
   ```bash
   cd embedded/proj_cm55
   make build
   make program
   ```

5. **运行嵌入式报警服务**（可选，用于调试）：
   ```bash
   python embedded/server/server.py --port 5000
   ```

### 云端

1. **安装依赖**：
   ```bash
   cd cloud/backend
   pip install -r requirements.txt
   ```

2. **配置数据库**：编辑 `cloud/backend/config.py`

3. **初始化数据库**：
   ```bash
   python cloud/backend/init_db.py
   ```

4. **启动后端**：
   ```bash
   python cloud/backend/run.py
   ```

5. **前端部署**：
   ```bash
   cd cloud/frontend
   npm install
   npm run build  # 产出在 dist/
   ```
   将 `dist/` 部署到 Nginx 或其他 Web 服务器

---

## 检测能力

| 类别 | 说明 | 报警优先级 | 语音提示 |
|------|------|:--------:|---------|
| steering_wheel | 方向盘定位 | - | - |
| hand | 手部检测 | - | - |
| hands_off | 双手离方向盘 | 最高 | "请双手抱紧方向盘" |
| no_seatbelt | 未系安全带 | 高 | "请系好安全带" |
| phone | 使用手机 | 中 | "驾驶中请勿使用手机" |
| yawn | 打哈欠/疲劳 | 低 | "你已疲劳请注意休息" |

---

## 数据流

```
USB摄像头 --> CM55 (YOLO推理) --> 共享内存 --> CM33 (网络)
                 |                    |              |
                 v                    v              v
            LCD 显示            语音播报       HTTP POST
                                               |
                                          +----v----+
                                          |  云端后端  |
                                          | POST /api |
                                          | /detection|
                                          | /upload   |
                                          +----+----+
                                               |
                                          +----v----+
                                          |  Vue前端  |
                                          | 仪表盘统计 |
                                          | 信用评分  |
                                          +---------+
```

---

## 技术栈

| 层 | 技术 |
|---|------|
| **AI 推理** | YOLOv5 + TFLM + Ethos-U55 NPU |
| **模型编译** | DEEPCRAFT Model Converter |
| **RTOS** | FreeRTOS (双核多任务) |
| **GPU** | VGLite (VeriSilicon) |
| **音频** | I2S/TDM + TLV320DAC3100 |
| **网络** | lwIP + mbedTLS + Wi-Fi WCM |
| **后端** | Flask + SQLAlchemy + JWT |
| **前端** | Vue 3 + Vite + ECharts |
| **数据库** | MySQL |
| **部署** | Nginx + Gunicorn + systemd |

---

## 开发历程

本项目从 Infineon 官方手势识别 Demo 深度定制而来：

1. 替换 YOLOv5 模型：手势分类 -> 驾驶员行为检测 (6类)
2. 新增双核共享内存通信协议
3. 新增 CM33 网络通信层 (Wi-Fi + JWT + HTTP)
4. 新增帧差唤醒省电机制
5. 新增 Hands-off 推断算法 (手与方向盘 IoU)
6. 新增 I2S 语音报警 (TLV320DAC3100)
7. 云端后端 (Flask REST API)
8. 前端仪表盘 (Vue.js + ECharts)
9. 用户管理 + 信用评分 + 黑名单系统

---

## License

MIT License
