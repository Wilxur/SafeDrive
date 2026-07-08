-- ============================================
-- 疲劳驾驶检测系统 - MySQL 数据库表结构
-- 货车信用管理平台版
-- ============================================

CREATE DATABASE IF NOT EXISTS car_mo DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE car_mo;

-- 物流公司表
CREATE TABLE IF NOT EXISTS companies (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(128) NOT NULL COMMENT '物流公司名称',
    contact_person VARCHAR(64) DEFAULT NULL COMMENT '联系人',
    phone VARCHAR(32) DEFAULT NULL COMMENT '联系电话',
    address VARCHAR(256) DEFAULT NULL COMMENT '地址',
    is_active TINYINT(1) NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- 用户表（货车司机/管理员）
CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(256) NOT NULL,
    role Enum('admin', 'driver') NOT NULL DEFAULT 'driver',
    real_name VARCHAR(64) DEFAULT NULL,
    email VARCHAR(128) DEFAULT NULL,
    device_id VARCHAR(128) DEFAULT NULL COMMENT '关联的设备ID',
    -- 物流体系扩展字段
    company_id BIGINT DEFAULT NULL COMMENT '所属物流公司',
    license_plate VARCHAR(32) DEFAULT NULL COMMENT '车牌号',
    credit_score INT NOT NULL DEFAULT 100 COMMENT '信用分 0-100',
    blacklisted TINYINT(1) NOT NULL DEFAULT 0 COMMENT '是否被拉黑',
    total_trips INT NOT NULL DEFAULT 0 COMMENT '累计出车次数',
    violation_count INT NOT NULL DEFAULT 0 COMMENT '违规次数',
    -- 原有字段
    is_active TINYINT(1) NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (company_id) REFERENCES companies(id) ON DELETE SET NULL,
    INDEX idx_company_id (company_id),
    INDEX idx_credit_score (credit_score),
    INDEX idx_blacklisted (blacklisted)
) ENGINE=InnoDB;

-- 检测记录表
CREATE TABLE IF NOT EXISTS detection_records (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    device_id VARCHAR(128) DEFAULT NULL,
    category ENUM('yawn','seatbelt','no_seatbelt','phone','steering_wheel','hand','hands_off') NOT NULL COMMENT '检测类别',
    confidence FLOAT DEFAULT NULL COMMENT '置信度',
    image_url VARCHAR(512) DEFAULT NULL COMMENT '检测截图URL',
    is_dangerous TINYINT(1) NOT NULL DEFAULT 0 COMMENT '是否危险行为',
    detected_at DATETIME NOT NULL COMMENT '检测时间',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_id (user_id),
    INDEX idx_category (category),
    INDEX idx_detected_at (detected_at),
    INDEX idx_is_dangerous (is_dangerous)
) ENGINE=InnoDB;

-- 危险驾驶统计表（按天汇总）
CREATE TABLE IF NOT EXISTS daily_stats (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    stat_date DATE NOT NULL,
    no_seatbelt_count INT NOT NULL DEFAULT 0,
    phone_count INT NOT NULL DEFAULT 0,
    seatbelt_count INT NOT NULL DEFAULT 0,
    yawn_count INT NOT NULL DEFAULT 0,
    hand_count INT NOT NULL DEFAULT 0,
    steering_wheel_count INT NOT NULL DEFAULT 0,
    closed_eyes_count INT NOT NULL DEFAULT 0,
    open_eyes_count INT NOT NULL DEFAULT 0,
    hands_off_count INT NOT NULL DEFAULT 0 COMMENT '双手离方向盘',
    total_dangerous INT NOT NULL DEFAULT 0,
    total_detections INT NOT NULL DEFAULT 0,
    credit_deducted INT NOT NULL DEFAULT 0 COMMENT '当日信用扣分',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE KEY uk_user_date (user_id, stat_date)
) ENGINE=InnoDB;

-- 信用分变更日志表
CREATE TABLE IF NOT EXISTS credit_logs (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    change INT NOT NULL COMMENT '变动值（负数为扣分）',
    score_after INT NOT NULL COMMENT '变动后信用分',
    reason VARCHAR(256) NOT NULL COMMENT '变动原因',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_id (user_id),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB;

-- 预警/提醒表
CREATE TABLE IF NOT EXISTS alerts (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    admin_id BIGINT NOT NULL COMMENT '发送预警的管理员',
    alert_type ENUM('warning','reminder','urgent','system') NOT NULL DEFAULT 'warning',
    title VARCHAR(256) NOT NULL,
    message TEXT NOT NULL,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (admin_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_read (user_id, is_read),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB;
