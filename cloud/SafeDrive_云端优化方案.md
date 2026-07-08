# SafeDrive 云端对齐嵌入端修改建议

> 嵌入端改动：不再上报 steering_wheel(4) 和 hand(5)，改为上报 hands_off(255)。
> 对照当前云端代码逐项列出需要修改的位置。

---

## P0：必须修改（板子发 hands_off 会导致 400/500 错误）

### 1. `backend/app/models.py` L83 — DetectionRecord ENUM

**当前**：
```python
category = db.Column(db.Enum('yawn', 'seatbelt', 'no_seatbelt', 'phone', 'steering_wheel', 'hand'), ...)
```

**改为**：
```python
category = db.Column(db.Enum('yawn', 'seatbelt', 'no_seatbelt', 'phone', 'steering_wheel', 'hand', 'hands_off'), ...)
```

**原因**：板子 POST `{"category":"hands_off",...}`，ENUM 里没有 → MySQL 拒绝写入 → 500 错误。

---

### 2. `backend/app/routes/detection.py` L34 — 有效类别校验

**当前**：
```python
valid = ['yawn', 'seatbelt', 'no_seatbelt', 'phone', 'steering_wheel', 'hand']
```

**改为**：
```python
valid = ['yawn', 'seatbelt', 'no_seatbelt', 'phone', 'hands_off']
```

**原因**：不加 `'hands_off'` → 板子发的会被 `return jsonify({'error': '无效的检测类别'}), 400` 拒绝。`'steering_wheel'` 和 `'hand'` 可保留做向后兼容。

---

### 3. `backend/app/routes/detection.py` L11 — 危险类别列表

**当前**：
```python
DANGEROUS_CATEGORIES = ['no_seatbelt', 'phone', 'yawn']
```

**改为**：
```python
DANGEROUS_CATEGORIES = ['no_seatbelt', 'phone', 'yawn', 'hands_off']
```

**原因**：hands_off 是严重违规，不加的话 is_dangerous=0 → 不扣分、不预警、不统计。

---

### 4. `backend/app/routes/admin.py` L12 — 扣分规则

**当前**：
```python
CREDIT_PENALTIES = {
    'phone': 5,
    'no_seatbelt': 3,
    'yawn': 2,
}
```

**改为**：
```python
CREDIT_PENALTIES = {
    'phone': 5,
    'no_seatbelt': 3,
    'yawn': 2,
    'hands_off': 5,      # 双手离方向盘，与手机同级
}
```

---

## P1：重要——统计和数据库

### 5. `backend/app/models.py` L110~128 — DailyStat 表

**当前**：没有 `hands_off_count` 列，有 `hand_count`、`steering_wheel_count`、`closed_eyes_count`、`open_eyes_count`

**改为**：新增一列：
```python
hands_off_count = db.Column(db.Integer, nullable=False, default=0)
```

并在 `to_dict()` 中返回：
```python
'hands_off_count': self.hands_off_count,
```

`hand_count` 和 `steering_wheel_count` 可保留（向后兼容旧数据），但不再有新增数据写入。`closed_eyes_count` 和 `open_eyes_count` 同理保留但无新数据。

---

### 6. `backend/app/routes/detection.py` L79~81 — 统计更新

**当前**：
```python
cat_field = category + '_count'
if hasattr(stat, cat_field):
    setattr(stat, cat_field, (getattr(stat, cat_field) or 0) + 1)
```

**不改**：这个动态拼接 `category + '_count'` 的逻辑是对的——hands_off 会拼接为 `'hands_off_count'`，只要 DailyStat 有这个列就能自动 +1。

---

### 7. `backend/app/routes/detection.py` stats API summary

需要在 `/api/detection/stats` 的 summary 查询里加上 `hands_off_count`：
```python
func.sum(DailyStat.hands_off_count).label('hands_off_count'),
```

---

### 8. `backend/app/routes/admin.py` L329~335 — 管理员仪表盘违规分布

**当前**：
```python
weekly_stats = db.session.query(
    func.sum(DailyStat.no_seatbelt_count).label('no_seatbelt'),
    func.sum(DailyStat.phone_count).label('phone'),
    func.sum(DailyStat.yawn_count).label('yawn'),
    func.sum(DailyStat.closed_eyes_count).label('closed_eyes'),  # ← 旧类别
    func.sum(DailyStat.hand_count).label('hand'),                 # ← 不再上报
).filter(DailyStat.stat_date >= week_ago).first()
```

**改为**：
```python
weekly_stats = db.session.query(
    func.sum(DailyStat.no_seatbelt_count).label('no_seatbelt'),
    func.sum(DailyStat.phone_count).label('phone'),
    func.sum(DailyStat.yawn_count).label('yawn'),
    func.sum(DailyStat.hands_off_count).label('hands_off'),
).filter(DailyStat.stat_date >= week_ago).first()
```

---

## P2：前端标签更新

### 9. 三个文件的 labels 映射

| 文件 | 行号 | 当前 | 改为 |
|------|------|------|------|
| `Dashboard.vue`（管理） | 179, 252 | `steering_wheel: '方向盘', hand: '手部'` | 删掉这两项，加 `hands_off: '双手离方向盘'` |
| `Records.vue`（用户） | 75 | `steering_wheel: '方向盘', hand: '手部'` | 同上 |
| `UserDetail.vue`（管理） | 241, 248, 258, 272, 458 | 多处 `hand_count`、`steering_wheel`、`closed_eyes` | 加 `hands_off_count`，清理旧类别 |

---

## 修改汇总表

| 优先级 | 文件 | 改动 |
|--------|------|------|
| P0 | `models.py` L83 | DetectionRecord ENUM 加 `'hands_off'` |
| P0 | `detection.py` L34 | valid 列表加 `'hands_off'` |
| P0 | `detection.py` L11 | DANGEROUS 加 `'hands_off'` |
| P0 | `admin.py` L12 | CREDIT_PENALTIES 加 `'hands_off': 5` |
| P1 | `models.py` L116 | DailyStat 加 `hands_off_count` 列 + to_dict |
| P1 | `detection.py` stats | summary 查询加 `hands_off_count` |
| P1 | `admin.py` L330 | weekly_stats 删 `closed_eyes`/`hand`，加 `hands_off` |
| P2 | `Dashboard.vue` | labels 更新 |
| P2 | `Records.vue` | labels 更新 |
| P2 | `UserDetail.vue` | 统计块更新 |
