-- ============================================
-- 货车信用管理平台 - 数据库迁移脚本
-- 兼容 MySQL 5.7 / 8.0 / MariaDB
-- 用法: mysql -u root -p car_mo < migration.sql
-- ============================================

-- 1. 创建物流公司表
CREATE TABLE IF NOT EXISTS companies (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(128) NOT NULL COMMENT '物流公司名称',
    contact_person VARCHAR(64) DEFAULT NULL,
    phone VARCHAR(32) DEFAULT NULL,
    address VARCHAR(256) DEFAULT NULL,
    is_active TINYINT(1) NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. 插入默认公司
INSERT IGNORE INTO companies (id, name, contact_person, phone, address)
VALUES (1, '默认物流公司', '系统管理员', '010-88888888', '北京市朝阳区');

-- 3. 创建信用分变更日志表
CREATE TABLE IF NOT EXISTS credit_logs (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    change INT NOT NULL COMMENT '变动值',
    score_after INT NOT NULL COMMENT '变动后信用分',
    reason VARCHAR(256) NOT NULL COMMENT '变动原因',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_user_id (user_id),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. users 表新增字段（逐条执行，忽略重复列错误）
-- MySQL / MariaDB 兼容写法
ALTER TABLE users ADD COLUMN company_id BIGINT DEFAULT NULL AFTER device_id;
ALTER TABLE users ADD COLUMN license_plate VARCHAR(32) DEFAULT NULL AFTER company_id;
ALTER TABLE users ADD COLUMN credit_score INT NOT NULL DEFAULT 100 AFTER license_plate;
ALTER TABLE users ADD COLUMN blacklisted TINYINT(1) NOT NULL DEFAULT 0 AFTER credit_score;
ALTER TABLE users ADD COLUMN total_trips INT NOT NULL DEFAULT 0 AFTER blacklisted;
ALTER TABLE users ADD COLUMN violation_count INT NOT NULL DEFAULT 0 AFTER total_trips;

-- 5. 给已有用户设置默认信用分（如果为0）
UPDATE users SET credit_score = 100 WHERE credit_score = 0 OR credit_score IS NULL;

-- 6. 添加外键约束
ALTER TABLE users ADD CONSTRAINT fk_user_company FOREIGN KEY (company_id) REFERENCES companies(id) ON DELETE SET NULL;

-- 7. daily_stats 表新增字段
ALTER TABLE daily_stats ADD COLUMN credit_deducted INT NOT NULL DEFAULT 0 AFTER total_detections;

-- 8. 添加索引
ALTER TABLE users ADD INDEX idx_company_id (company_id);
ALTER TABLE users ADD INDEX idx_credit_score (credit_score);
ALTER TABLE users ADD INDEX idx_blacklisted (blacklisted);