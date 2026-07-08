"""
数据库初始化脚本
运行方式: python init_db.py
注意: 需要先创建数据库 car_mo
"""
from app import create_app, db
from app.models import User, Company, CreditLog
from werkzeug.security import generate_password_hash
from datetime import datetime

app = create_app()

with app.app_context():
    db.create_all()
    print('数据库表已创建')

    # 创建默认物流公司
    company = Company.query.filter_by(name='默认物流公司').first()
    if not company:
        company = Company(
            name='默认物流公司',
            contact_person='系统管理员',
            phone='010-88888888',
            address='北京市朝阳区',
        )
        db.session.add(company)
        db.session.commit()
        print('默认物流公司已创建')
    else:
        print('默认物流公司已存在')

    # 创建管理员
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
        print('默认管理员已创建: admin / admin123')
    else:
        print('管理员账户已存在')

    # 创建测试司机（关联到公司）
    test_user = User.query.filter_by(username='test').first()
    if not test_user:
        test_user = User(
            username='test',
            password_hash=generate_password_hash('test123'),
            role='driver',
            real_name='张三',
            device_id='DEVICE-001',
            company_id=company.id,
            license_plate='京A·88888',
            credit_score=85,
        )
        db.session.add(test_user)
        db.session.commit()

        # 添加初始信用日志
        log = CreditLog(
            user_id=test_user.id,
            change=0,
            score_after=85,
            reason='初始信用分',
        )
        db.session.add(log)
        db.session.commit()
        print('测试司机已创建: test / test123 (信用分85, 车牌京A·88888)')
    else:
        print('测试用户已存在')

    print('\n数据库初始化完成!')
    print('管理员: admin / admin123')
    print('测试司机: test / test123 (信用分85)')