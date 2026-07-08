# SafeDrive 安全驾驶检测系统

> 基于 Infineon PSOC Edge E84 双核 MCU 的嵌入式 AI 驾驶员监控系统（DMS）

---

## 一、项目背景

随着智能座舱和汽车安全技术的发展，实时监测驾驶员状态成为预防交通事故的重要手段。疲劳驾驶、分心驾驶（使用手机）、未系安全带、双手脱离方向盘等行为是交通事故的主要诱因。

SafeDrive 是一款**端侧 AI 驾驶员监控系统**，运行在 Infineon PSOC Edge E84 边缘 AI MCU 上，通过车载摄像头实时采集视频画面，利用 YOLOv5 目标检测模型识别驾驶员行为，并在本地完成报警（语音播报 + LCD 显示），同时通过 Wi-Fi 将检测数据上传至云端服务器。

本项目从 Infineon 官方**手势识别 Demo** 深度定制而来，将其改造为完整的驾驶员监控解决方案。

---

## 二、硬件平台

| 组件 | 型号 | 用途 |
|------|------|------|
| 主控芯片 | Infineon PSOC Edge E84 (PSE84) | 双核异构 MCU，集成 NPU + GPU |
| CPU 架构 | CM33 (250MHz) + CM55 (500MHz) | CM55 跑 AI 推理，CM33 跑网络通信 |
| AI 加速器 | Ethos-U55 NPU | 神经网络硬件加速 |
| 外部内存 | 8MB PSRAM (QSPI) | 模型权重 + 图像缓冲 |
| 摄像头 | USB UVC (支持 4 款) | 320×240 实时视频采集 |
| 显示屏 | Waveshare 4.3" DSI LCD | 800×480，显示检测结果 |
| 音频 DAC | TLV320DAC3100 (I2S) | 语音报警输出至板载扬声器 |
| 无线 | 板载 Wi-Fi (SDIO) | STA 模式连接云服务器 |

---

## 三、系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                      PSOC Edge E84                           │
│                                                              │
│  ┌──────────────────┐          ┌──────────────────┐         │
│  │     CM55 核心      │  共享内存 │     CM33 核心      │         │
│  │   (500MHz)        │◄────────►│   (250MHz)        │         │
│  │                    │ 0x26480000│                    │         │
│  │  USB 摄像头采集     │          │  Wi-Fi STA 连接    │         │
│  │  YOLOv5 目标检测   │          │  JWT 认证登录      │         │
│  │  NMS 后处理        │          │  HTTP 数据上报     │         │
│  │  LCD 渲染 (GPU)    │          │  速率限制 + 去重   │         │
│  │  I2S 语音播报      │          │                    │         │
│  │  帧差唤醒 (省电)   │          │                    │         │
│  └──────────────────┘          └──────────────────┘         │
│           │                              │                   │
│           ▼                              ▼                   │
│    Waveshare 4.3" LCD            Flask 云服务器               │
│    TLV320DAC3100 扬声器           POST /api/dms/alarm         │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、检测能力

| 检测目标 | 说明 | 报警优先级 |
|---------|------|-----------|
| 🚗 方向盘 (steering_wheel) | 定位方向盘位置 | — |
| ✋ 手 (hand) | 检测手的位置 | — |
| ⚠️ 手离方向盘 (hands_off) | 手与方向盘 IoU < 0.1 推断 | **最高** |
| 🪢 未系安全带 (no_seatbelt) | 检测安全带状态 | **高** |
| 📱 打电话 (phone) | 检测手持电话行为 | **中** |
| 🥱 打哈欠 (yawn) | 疲劳驾驶检测 | **低** |

---

## 五、项目亮点

### 1. 双核异构协同

- **CM55**（500MHz + Ethos-U55 NPU）：专注 AI 推理，运行 YOLOv5 目标检测模型
- **CM33**（250MHz）：处理 Wi-Fi 联网、JWT 认证、云端通信
- 通过 **256KB 共享内存**（地址 0x26480000）实现核间无锁通信，使用 `__DMB()` 内存屏障保证缓存一致性

### 2. YOLOv5 端侧推理

- 模型输入：224×224×3，输出：1029 个候选框 × 10 属性
- 通过 DEEPCRAFT Model Converter 编译部署，利用 Ethos-U55 NPU 硬件加速
- 6 类目标检测 + NMS 后处理（IoU 阈值 0.35）
- 推理时间约 10-20ms/帧，支持实时检测

### 3. 帧差唤醒省电机制

- 将 224×224 图像下采样为 **40×40 灰度网格**
- 逐帧计算平均像素差异
- **连续 3 帧**有运动 → 唤醒 ACTIVE 模式（跑 YOLO）
- **连续 30 帧**静止 + 无检测目标 → 休眠 IDLE 模式（跳过推理）
- 大幅降低平均功耗，适合车载常电场景

### 4. I2S 语音报警

- 通过 TDM0 (I2S) → **TLV320DAC3100** 音频 DAC → 板载扬声器
- 5 段预录制中文语音提示：
  - "系统已就绪"
  - "请握好方向盘"
  - "请勿使用手机"
  - "请勿疲劳驾驶"
  - "请系好安全带"
- 16kHz / 16-bit / 单声道 PCM，ISR 驱动播放，非阻塞设计不卡推理

### 5. GPU 硬件加速显示

- 使用 VGLite GPU 进行 **YUYV → BGR565** 色彩空间转换
- 硬件 blit 缩放（320×240 → 800×480）
- 检测框 + 类别标签 + 置信度叠加显示
- 实时显示 ACTIVE/IDLE 状态 + 推理耗时 + 云端信用分

### 6. 云端联网

- CM33 通过 Wi-Fi STA 连接云端服务器
- JWT Bearer Token 认证（12 小时自动刷新）
- 每个检测框独立 POST 上报 `/api/detection/upload`
- 速率限制（200ms 间隔，每帧最多 5 条）
- 云端返回 `credit_score` + `blacklisted`，回写共享内存影响 LCD 显示

### 7. 鲁棒性设计

- USB 摄像头热插拔 + 自动错误恢复（10 次连续错误重启流）
- 帧超时监控（10 秒无帧强制重置）
- Wi-Fi 自动重连（最多 10 次）
- TCP 断线自动重连（JWT Token 跨连接保持有效）

### 8. 自适应模型适配

- 运行时检测模型输出 shape（attr-major vs box-major），兼容新旧模型格式
- 低样本类别（wheel、hand、seatbelt）使用更低置信度阈值（0.15），提高召回率

---

## 六、技术栈

| 层级 | 技术 |
|------|------|
| 深度学习框架 | TensorFlow 2.19 + Ethos-U Vela 4.5 |
| 模型编译器 | DEEPCRAFT Model Converter (ImagiNet Compiler 5.10) |
| 推理引擎 | TensorFlow Lite for Microcontrollers (TFLM) |
| RTOS | FreeRTOS (双核多任务) |
| GPU 渲染 | VGLite (VeriSilicon GPU) |
| 显示 | LVGL + Waveshare 4.3" DSI |
| 音频 | I2S/TDM0 + TLV320DAC3100 |
| 网络 | lwIP + mbedTLS + Wi-Fi Connection Manager |
| 云端 | Flask (Python) REST API |
| 工具链 | ModusToolbox 3.7 + GCC ARM 14.2.1 |

---

## 七、目录结构

```
SafeDrive_DMS/
├── proj_cm33_s/          # CM33 Secure（安全启动）
├── proj_cm33_ns/         # CM33 Non-Secure（网络通信）
│   ├── main.c            # 入口：初始化 → 启动CM55 → 创建TCP任务
│   ├── tcp_client.c      # Wi-Fi / JWT / 共享内存轮询 / HTTP上报
│   └── dms_config.h      # Wi-Fi/服务器/设备凭证配置
├── proj_cm55/            # CM55（AI 推理核心）
│   ├── source/
│   │   ├── main.c        # 入口：创建 3 个RTOS任务
│   │   ├── inference_task.c  # YOLO推理 + NMS + 帧差唤醒 + 语音触发
│   │   ├── lcd_task.c    # VGLite GPU渲染 + 检测框显示
│   │   ├── usb_camera_task.c # USB UVC摄像头驱动
│   │   ├── voice_player.c    # I2S语音播报模块
│   │   └── voice_*.h     # 5段预录制语音PCM数据
│   └── model/
│       └── model.c/h     # DEEPCRAFT编译的YOLOv5模型
├── server/
│   └── server.py         # Flask云端报警服务器
├── shared_dms.h          # 双核共享内存协议
└── docs/
    └── design_and_implementation.md
```

---

## 八、开发历程

项目从 Infineon 官方 **石头-剪刀-布手势识别 Demo** 开始，经过以下定制改造：

1. 替换 DEEPCRAFT 模型：手势分类 → YOLOv5 驾驶员行为检测
2. 新增双核共享内存通信协议
3. 新增 CM33 网络通信层（Wi-Fi + JWT + HTTP）
4. 新增帧差唤醒省电机制
5. 新增 Hands-off 推断算法（手与方向盘 IoU 判定）
6. 新增 I2S 语音报警（TLV320DAC3100 驱动）
7. 新增云端 Flask 服务器
8. 新增 LCD 状态显示（ACTIVE/IDLE、推理耗时、信用分）
9. 移除蜂鸣器，改为全程语音播报

---

> **项目状态**：完整管线已打通 — 摄像头采集 → AI 推理 → LCD 显示 → 语音报警 → 云端上传 全链路工作正常。
