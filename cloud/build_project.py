import os

BASE = r"C:\Users\ge138\Documents\疲劳驾驶"

def write_file(rel_path, content):
    full = os.path.join(BASE, rel_path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  Created: {rel_path}")

# ===== database/schema.sql =====
s = ""
s += "-- ============================================\n"
s += "-- 疲劳驾驶检测系统 - MySQL 数据库表结构\n"
s += "-- ============================================\n"
s += "\n"
s += "CREATE DATABASE IF NOT EXISTS fatigue_driving DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n"
s += "\n"
s += "USE fatigue_driving;\n"
