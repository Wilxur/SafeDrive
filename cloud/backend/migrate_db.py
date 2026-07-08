"""
数据库迁移脚本 - 安全迁移到新版本
运行方式: python migrate_db.py
兼容 MySQL 5.7/8.0 和 MariaDB
"""
from app import create_app, db
from sqlalchemy import inspect, text
from app.models import Company, User, CreditLog
from werkzeug.security import generate_password_hash
import logging

logging.basicConfig(level=logging.INFO, format='%(message)s')
log = logging.getLogger(__name__)

app = create_app()

with app.app_context():
    inspector = inspect(db.engine)
    conn = db.engine.connect()

    # 1. 获取现有表
    tables = inspector.get_table_names()
    log.info(f'现有表: {tables}')

    # 2. 创建 companies 表
    if 'companies' not in tables:
        db.create_all()
        log.info('✓ 创建 companies 表')
    else:
        log.info('✓ companies 表已存在')

    # 3. 添加 users 表新字段
    user_columns = [col['name'] for col in inspector.get_columns('users')]
    new_fields = {
        'company_id': 'BIGINT DEFAULT NULL',
        'license_plate': 'VARCHAR(32) DEFAULT NULL',
        'credit_score': 'INT NOT NULL DEFAULT 100',
        'blacklisted': 'TINYINT(1) NOT NULL DEFAULT 0',
        'total_trips': 'INT NOT NULL DEFAULT 0',
        'violation_count': 'INT NOT NULL DEFAULT 0',
    }
    for field, dtype in new_fields.items():
        if field not in user_columns:
            conn.execute(text(f'ALTER TABLE users ADD COLUMN {field} {dtype}'))
            log.info(f'  ✓ users 添加字段: {field}')
        else:
            log.info(f'  - users.{field} 已存在')

    # 4. 添加 daily_stats 表新字段
    if 'daily_stats' in tables:
        stat_columns = [col['name'] for col in inspector.get_columns('daily_stats')]
        if 'credit_deducted' not in stat_columns:
            conn.execute(text('ALTER TABLE daily_stats ADD COLUMN credit_deducted INT NOT NULL DEFAULT 0'))
            log.info('✓ daily_stats 添加字段: credit_deducted')
        else:
            log.info('- daily_stats.credit_deducted 已存在')

    # 5. 创建 credit_logs 表
    if 'credit_logs' not in tables:
        conn.execute(text('''
            CREATE TABLE IF NOT EXISTS credit_logs (
                id BIGINT AUTO_INCREMENT PRIMARY KEY,
                user_id BIGINT NOT NULL,
                change INT NOT NULL,
                score_after INT NOT NULL,
                reason VARCHAR(256) NOT NULL,
                created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                INDEX idx_user_id (user_id),
                INDEX idx_created_at (created_at)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        '''))
        log.info('✓ 创建 credit_logs 表')
    else:
        log.info('✓ credit_logs 表已存在')

    # 5.5 更新 role 枚举（user -> driver）
    # 先批量更新已有数据
    result = conn.execute(text("UPDATE users SET role = 'driver' WHERE role = 'user'"))
    log.info(f"✓ 已更新 {result.rowcount} 条 role='user' 改为 'driver'")
    # 再修改枚举定义
    try:
        conn.execute(text("ALTER TABLE users MODIFY COLUMN role ENUM('admin', 'driver') NOT NULL DEFAULT 'driver'"))
        log.info("✓ role 枚举已更新: admin/driver")
    except Exception as e:
        log.info(f"- role 枚举更新（可能已是最新）: {e}")
    # 6. 设置已有用户信用分
    conn.execute(text("UPDATE users SET credit_score = 100 WHERE credit_score IS NULL OR credit_score = 0"))
    log.info('✓ 已有用户信用分已设为100')

    # 7. 创建默认公司
    company = Company.query.filter_by(id=1).first()
    if not company:
        company = Company(
            id=1,
            name='默认物流公司',
            contact_person='系统管理员',
            phone='010-88888888',
            address='北京市朝阳区',
        )
        db.session.add(company)
        db.session.commit()
        log.info('✓ 创建默认物流公司')
    else:
        log.info('✓ 默认公司已存在')

    # 8. 确保管理员和测试用户存在
    admin = User.query.filter_by(username='admin').first()
    if not admin:
        admin = User(
            username='admin',
            password_hash=generate_password_hash('admin123'),
            role='admin',
            real_name='系统管理员',
            credit_score=100,
        )
        db.session.add(admin)
        db.session.commit()
        log.info('✓ 创建管理员: admin / admin123')
    else:
        log.info('✓ 管理员已存在')

    test_user = User.query.filter_by(username='test').first()
    if not test_user:
        test_user = User(
            username='test',
            password_hash=generate_password_hash('test123'),
            role='driver',
            real_name='张三',
            device_id='DEVICE-001',
            company_id=1,
            license_plate='京A·88888',
            credit_score=85,
        )
        db.session.add(test_user)
        db.session.commit()
        log.info('✓ 创建测试司机: test / test123 (信用分85)')
    else:
        log.info('✓ 测试用户已存在')

    # 9. 添加外键约束
    try:
        conn.execute(text(
            'ALTER TABLE users ADD CONSTRAINT fk_user_company '
            'FOREIGN KEY (company_id) REFERENCES companies(id) ON DELETE SET NULL'
        ))
        log.info('✓ 添加外键约束')
    except Exception:
        log.info('- 外键约束已存在')

    # 10. 更新 detection_records category 枚举（8类→6类）
    if 'detection_records' in tables:
        try:
            conn.execute(text("UPDATE detection_records SET category = 'yawn' WHERE category = 'closed_eyes'"))
            conn.execute(text("UPDATE detection_records SET category = 'seatbelt' WHERE category = 'open_eyes'"))
            log.info('✓ 已迁移检测类别: closed_eyes->yawn, open_eyes->seatbelt')
            conn.execute(text("ALTER TABLE detection_records MODIFY COLUMN category ENUM('yawn','seatbelt','no_seatbelt','phone','steering_wheel','hand') NOT NULL COMMENT '检测类别'"))
            log.info('✓ 检测类别枚举已更新为6类')
        except Exception as e:
            log.info(f'- 检测类别枚举更新（可能已是最新）: {e}')

    # 10.5 添加 daily_stats.hands_off_count 字段
    if 'daily_stats' in tables:
        stat_columns = [col['name'] for col in inspector.get_columns('daily_stats')]
        if 'hands_off_count' not in stat_columns:
            conn.execute(text('ALTER TABLE daily_stats ADD COLUMN hands_off_count INT NOT NULL DEFAULT 0 COMMENT \'双手离方向盘\''))
            log.info('✓ daily_stats 添加字段: hands_off_count')
        else:
            log.info('- daily_stats.hands_off_count 已存在')

    conn.commit()
    log.info('\n✅ 数据库迁移完成!')

    log.info('   管理员: admin / admin123')
    log.info('   测试司机: test / test123')