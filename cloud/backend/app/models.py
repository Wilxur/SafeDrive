from app import db
from datetime import datetime


class Company(db.Model):
    __tablename__ = 'companies'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    name = db.Column(db.String(128), nullable=False, comment='物流公司名称')
    contact_person = db.Column(db.String(64), comment='联系人')
    phone = db.Column(db.String(32), comment='联系电话')
    address = db.Column(db.String(256), comment='地址')
    is_active = db.Column(db.SmallInteger, nullable=False, default=1)
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)
    updated_at = db.Column(db.DateTime, nullable=False, default=datetime.now, onupdate=datetime.now)

    drivers = db.relationship('User', backref='company', lazy='dynamic',
                              foreign_keys='User.company_id')

    def to_dict(self):
        return {
            'id': self.id,
            'name': self.name,
            'contact_person': self.contact_person,
            'phone': self.phone,
            'address': self.address,
            'is_active': self.is_active,
            'created_at': self.created_at.isoformat() if self.created_at else None,
        }


class User(db.Model):
    __tablename__ = 'users'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    username = db.Column(db.String(64), unique=True, nullable=False)
    password_hash = db.Column(db.String(256), nullable=False)
    role = db.Column(db.Enum('admin', 'driver'), nullable=False, default='driver')
    real_name = db.Column(db.String(64))
    email = db.Column(db.String(128))
    device_id = db.Column(db.String(128), comment='关联的设备ID')
    # 物流公司关联 & 货车司机信用体系
    company_id = db.Column(db.BigInteger, db.ForeignKey('companies.id'), comment='所属物流公司')
    license_plate = db.Column(db.String(32), comment='车牌号（司机驾驶的货车）')
    credit_score = db.Column(db.Integer, nullable=False, default=100, comment='信用分 0-100，初始100')
    blacklisted = db.Column(db.SmallInteger, nullable=False, default=0, comment='是否被拉黑')
    total_trips = db.Column(db.Integer, nullable=False, default=0, comment='累计出车次数')
    violation_count = db.Column(db.Integer, nullable=False, default=0, comment='违规次数')
    is_active = db.Column(db.SmallInteger, nullable=False, default=1)
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)
    updated_at = db.Column(db.DateTime, nullable=False, default=datetime.now, onupdate=datetime.now)

    detection_records = db.relationship('DetectionRecord', backref='user', lazy='dynamic')
    alerts_received = db.relationship('Alert', foreign_keys='Alert.user_id', backref='user', lazy='dynamic')
    credit_logs = db.relationship('CreditLog', backref='user', lazy='dynamic')

    def to_dict(self):
        return {
            'id': self.id,
            'username': self.username,
            'role': self.role,
            'real_name': self.real_name,
            'email': self.email,
            'device_id': self.device_id,
            'company_id': self.company_id,
            'company_name': self.company.name if self.company else None,
            'license_plate': self.license_plate,
            'credit_score': self.credit_score,
            'blacklisted': self.blacklisted,
            'total_trips': self.total_trips,
            'violation_count': self.violation_count,
            'is_active': self.is_active,
            'created_at': self.created_at.isoformat() if self.created_at else None,
        }


class DetectionRecord(db.Model):
    __tablename__ = 'detection_records'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    user_id = db.Column(db.BigInteger, db.ForeignKey('users.id'), nullable=False)
    device_id = db.Column(db.String(128))
    category = db.Column(db.Enum('yawn', 'seatbelt', 'no_seatbelt', 'phone', 'steering_wheel', 'hand', 'hands_off'), nullable=False, comment='检测类别')
    confidence = db.Column(db.Float, comment='置信度')
    image_url = db.Column(db.String(512), comment='检测截图URL')
    is_dangerous = db.Column(db.SmallInteger, nullable=False, default=0, comment='是否危险行为')
    detected_at = db.Column(db.DateTime, nullable=False, comment='检测时间')
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)

    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'device_id': self.device_id,
            'category': self.category,
            'confidence': self.confidence,
            'image_url': self.image_url,
            'is_dangerous': self.is_dangerous,
            'detected_at': self.detected_at.isoformat() if self.detected_at else None,
            'created_at': self.created_at.isoformat() if self.created_at else None,
        }

    def to_admin_dict(self):
        """管理员视图 - 不暴露置信度"""
        d = self.to_dict()
        d.pop('confidence', None)
        return d


class DailyStat(db.Model):
    __tablename__ = 'daily_stats'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    user_id = db.Column(db.BigInteger, db.ForeignKey('users.id'), nullable=False)
    stat_date = db.Column(db.Date, nullable=False)
    no_seatbelt_count = db.Column(db.Integer, nullable=False, default=0)
    phone_count = db.Column(db.Integer, nullable=False, default=0)
    seatbelt_count = db.Column(db.Integer, nullable=False, default=0)
    yawn_count = db.Column(db.Integer, nullable=False, default=0)
    hand_count = db.Column(db.Integer, nullable=False, default=0)
    steering_wheel_count = db.Column(db.Integer, nullable=False, default=0)
    closed_eyes_count = db.Column(db.Integer, nullable=False, default=0)
    open_eyes_count = db.Column(db.Integer, nullable=False, default=0)
    hands_off_count = db.Column(db.Integer, nullable=False, default=0, comment='双手离方向盘')
    total_dangerous = db.Column(db.Integer, nullable=False, default=0)
    total_detections = db.Column(db.Integer, nullable=False, default=0)
    # 信用分相关：当日扣分
    credit_deducted = db.Column(db.Integer, nullable=False, default=0, comment='当日信用扣分')
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)
    updated_at = db.Column(db.DateTime, nullable=False, default=datetime.now, onupdate=datetime.now)

    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'stat_date': self.stat_date.isoformat() if self.stat_date else None,
            'no_seatbelt_count': self.no_seatbelt_count,
            'phone_count': self.phone_count,
            'seatbelt_count': self.seatbelt_count,
            'yawn_count': self.yawn_count,
            'hand_count': self.hand_count,
            'steering_wheel_count': self.steering_wheel_count,
            'closed_eyes_count': self.closed_eyes_count,
            'open_eyes_count': self.open_eyes_count,
            'total_dangerous': self.total_dangerous,
            'total_detections': self.total_detections,
            'credit_deducted': self.credit_deducted,
        }


class Alert(db.Model):
    __tablename__ = 'alerts'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    user_id = db.Column(db.BigInteger, db.ForeignKey('users.id'), nullable=False)
    admin_id = db.Column(db.BigInteger, db.ForeignKey('users.id'), nullable=False, comment='发送预警的管理员')
    alert_type = db.Column(db.Enum('warning', 'reminder', 'urgent', 'system'), nullable=False, default='warning')
    title = db.Column(db.String(256), nullable=False)
    message = db.Column(db.Text, nullable=False)
    is_read = db.Column(db.SmallInteger, nullable=False, default=0)
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)

    admin = db.relationship('User', foreign_keys=[admin_id])

    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'admin_id': self.admin_id,
            'admin_name': self.admin.real_name or self.admin.username if self.admin else None,
            'alert_type': self.alert_type,
            'title': self.title,
            'message': self.message,
            'is_read': self.is_read,
            'created_at': self.created_at.isoformat() if self.created_at else None,
        }


class CreditLog(db.Model):
    """信用分变更记录"""
    __tablename__ = 'credit_logs'

    id = db.Column(db.BigInteger, primary_key=True, autoincrement=True)
    user_id = db.Column(db.BigInteger, db.ForeignKey('users.id'), nullable=False, index=True)
    change = db.Column(db.Integer, nullable=False, comment='变动值（负数为扣分）')
    score_after = db.Column(db.Integer, nullable=False, comment='变动后信用分')
    reason = db.Column(db.String(256), nullable=False, comment='变动原因')
    created_at = db.Column(db.DateTime, nullable=False, default=datetime.now)

    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'change': self.change,
            'score_after': self.score_after,
            'reason': self.reason,
            'created_at': self.created_at.isoformat() if self.created_at else None,
        }
