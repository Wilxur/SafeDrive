from flask import Blueprint, request, jsonify
from flask_jwt_extended import jwt_required, get_jwt_identity, get_jwt
from app import db
from app.models import User, DetectionRecord, DailyStat, Alert, CreditLog, Company
from datetime import date, timedelta
from sqlalchemy import func
from functools import wraps

admin_bp = Blueprint('admin', __name__)

# ========== 信用分扣分规则 ==========
CREDIT_PENALTIES = {
    'phone': 5,       # 使用手机 -5
    'no_seatbelt': 3, # 未系安全带 -3
    'yawn': 2,        # 打哈欠（疲劳驾驶） -2
    'hands_off': 5,    # 双手离方向盘 -5
}

# ========== 安全驾驶加分规则（云端优化方案） ==========
CREDIT_REWARDS = {
    'daily_safe': 1,      # 当日零违规 +1
    'weekly_clean': 3,    # 连续7天无违规 +3
    'daily_active': 1,    # 每日设备上线 +1
}

# ========== 分级预警阈值（云端优化方案） ==========
CREDIT_LEVELS = {
    'excellent':    {'min': 90, 'label': '优秀', 'color': 'green'},
    'good':         {'min': 70, 'label': '良好', 'color': 'blue'},
    'fair':         {'min': 50, 'label': '一般', 'color': 'yellow'},
    'warning':      {'min': 40, 'label': '警告', 'color': 'orange'},
    'blacklist':    {'min': 30, 'label': '危险', 'color': 'red'},
}


def update_credit_score(user, category):
    """根据检测类别自动更新信用分"""
    penalty = CREDIT_PENALTIES.get(category, 0)
    if penalty == 0:
        return

    old_score = user.credit_score
    new_score = max(0, old_score - penalty)
    user.credit_score = new_score
    user.violation_count = (user.violation_count or 0) + 1

    # 记录信用分变更
    log = CreditLog(
        user_id=user.id,
        change=-penalty,
        score_after=new_score,
        reason=f'检测到{category}，扣除{penalty}信用分',
    )
    db.session.add(log)

    # 信用分低于30自动拉黑
    if new_score <= 30 and not user.blacklisted:
        user.blacklisted = 1
        alert = Alert(
            user_id=user.id,
            admin_id=1,
            alert_type='system',
            title='信用分过低，已被自动拉黑',
            message=f'您的信用分已降至{new_score}分，低于安全阈值，已被系统自动加入黑名单。请联系管理员。',
        )
        db.session.add(alert)


def admin_required(f):
    @wraps(f)
    @jwt_required()
    def decorated(*args, **kwargs):
        claims = get_jwt()
        if claims.get('role') != 'admin':
            return jsonify({'error': '需要管理员权限'}), 403
        return f(*args, **kwargs)
    return decorated


# ==================== 公司管理 ====================

@admin_bp.route('/companies', methods=['GET'])
@admin_required
def get_companies():
    companies = Company.query.order_by(Company.id).all()
    result = []
    for c in companies:
        d = c.to_dict()
        d['driver_count'] = User.query.filter_by(company_id=c.id, role='driver').count()
        result.append(d)
    return jsonify({'companies': result}), 200


@admin_bp.route('/companies', methods=['POST'])
@admin_required
def create_company():
    data = request.get_json()
    if not data or not data.get('name'):
        return jsonify({'error': '请输入公司名称'}), 400
    company = Company(
        name=data['name'],
        contact_person=data.get('contact_person'),
        phone=data.get('phone'),
        address=data.get('address'),
    )
    db.session.add(company)
    db.session.commit()
    return jsonify({'message': '公司已创建', 'company': company.to_dict()}), 201


@admin_bp.route('/companies/<int:company_id>', methods=['PUT'])
@admin_required
def update_company(company_id):
    company = Company.query.get(company_id)
    if not company:
        return jsonify({'error': '公司不存在'}), 404
    data = request.get_json()
    for field in ['name', 'contact_person', 'phone', 'address']:
        if field in data:
            setattr(company, field, data[field])
    db.session.commit()
    return jsonify({'message': '已更新', 'company': company.to_dict()}), 200


@admin_bp.route('/companies/<int:company_id>', methods=['DELETE'])
@admin_required
def delete_company(company_id):
    company = Company.query.get(company_id)
    if not company:
        return jsonify({'error': '公司不存在'}), 404
    # 检查是否有司机关联
    if User.query.filter_by(company_id=company_id).count() > 0:
        return jsonify({'error': '该公司下还有司机，无法删除'}), 400
    db.session.delete(company)
    db.session.commit()
    return jsonify({'message': '已删除'}), 200


# ==================== 用户管理 ====================

@admin_bp.route('/users', methods=['GET'])
@admin_required
def get_users():
    users = User.query.filter_by(role='driver').all()
    return jsonify({'users': [u.to_dict() for u in users]}), 200



@admin_bp.route('/users/<int:user_id>', methods=['PUT'])
@admin_required
def update_user(user_id):
    """更新用户信息（分配公司、车牌号等）"""
    data = request.get_json()
    if not data:
        return jsonify({'error': '请提供更新数据'}), 400

    user = User.query.get(int(user_id))
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    if 'company_id' in data:
        company = Company.query.get(data['company_id']) if data['company_id'] else None
        user.company_id = data['company_id']
    if 'license_plate' in data:
        user.license_plate = data['license_plate']
    if 'real_name' in data:
        user.real_name = data['real_name']
    if 'email' in data:
        user.email = data['email']
    if 'device_id' in data:
        user.device_id = data['device_id']

    db.session.commit()
    return jsonify({'message': '用户信息已更新', 'user': user.to_dict()}), 200


@admin_bp.route('/users/<int:user_id>', methods=['GET'])
@admin_required
def get_user_detail(user_id):
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    latest = DetectionRecord.query.filter_by(user_id=user_id).order_by(DetectionRecord.detected_at.desc()).first()
    today = date.today()
    today_stat = DailyStat.query.filter_by(user_id=user_id, stat_date=today).first()
    unread_count = Alert.query.filter_by(user_id=user_id, is_read=0).count()
    credit_logs = CreditLog.query.filter_by(user_id=user_id).order_by(CreditLog.created_at.desc()).limit(20).all()

    return jsonify({
        'user': user.to_dict(),
        'latest_detection': latest.to_admin_dict() if latest else None,
        'today_stat': today_stat.to_dict() if today_stat else None,
        'unread_alerts': unread_count,
        'credit_logs': [l.to_dict() for l in credit_logs],
    }), 200


@admin_bp.route('/users/<int:user_id>/blacklist', methods=['POST'])
@admin_required
def toggle_blacklist(user_id):
    """拉黑/解封用户"""
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    data = request.get_json() or {}
    action = data.get('action', 'toggle')

    if action == 'blacklist':
        user.blacklisted = 1
        msg = '已加入黑名单'
    elif action == 'unblacklist':
        user.blacklisted = 0
        msg = '已移出黑名单'
    else:
        user.blacklisted = 1 - user.blacklisted
        msg = '已加入黑名单' if user.blacklisted else '已移出黑名单'

    # 加入黑名单时自动发送通知
    if user.blacklisted:
        alert = Alert(
            user_id=user.id,
            admin_id=int(get_jwt_identity()),
            alert_type='urgent',
            title='已被加入黑名单',
            message=f'您因信用分过低或违规行为已被管理员加入黑名单，请联系管理员处理。当前信用分：{user.credit_score}',
        )
        db.session.add(alert)

    db.session.commit()
    return jsonify({'message': msg, 'blacklisted': user.blacklisted}), 200


@admin_bp.route('/users/<int:user_id>/credit', methods=['POST'])
@admin_required
def adjust_credit(user_id):
    """管理员手动调整信用分"""
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    data = request.get_json()
    if not data or 'change' not in data:
        return jsonify({'error': '请提供变动值'}), 400

    change = int(data['change'])
    old_score = user.credit_score
    new_score = max(0, min(100, old_score + change))
    user.credit_score = new_score

    log = CreditLog(
        user_id=user.id,
        change=change,
        score_after=new_score,
        reason=data.get('reason', '管理员手动调整'),
    )
    db.session.add(log)
    db.session.commit()
    return jsonify({
        'message': '信用分已调整',
        'credit_score': new_score,
        'change': change,
        'credit_log': log.to_dict(),
    }), 200


@admin_bp.route('/users/<int:user_id>/history', methods=['GET'])
@admin_required
def get_user_history(user_id):
    days = request.args.get('days', 7, type=int)
    start_date = date.today() - timedelta(days=days - 1)
    stats = DailyStat.query.filter(
        DailyStat.user_id == user_id,
        DailyStat.stat_date >= start_date
    ).order_by(DailyStat.stat_date.asc()).all()
    return jsonify({'daily_stats': [s.to_dict() for s in stats]}), 200


@admin_bp.route('/users/<int:user_id>/records', methods=['GET'])
@admin_required
def get_user_records(user_id):
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)
    dangerous_only = request.args.get('dangerous_only', 0, type=int)

    query = DetectionRecord.query.filter_by(user_id=user_id)
    if dangerous_only:
        query = query.filter_by(is_dangerous=1)

    query = query.order_by(DetectionRecord.detected_at.desc())
    pagination = query.paginate(page=page, per_page=per_page, error_out=False)

    return jsonify({
        'records': [r.to_admin_dict() for r in pagination.items],
        'total': pagination.total,
        'page': page,
        'per_page': per_page,
        'pages': pagination.pages,
    }), 200


# ==================== 预警管理 ====================

@admin_bp.route('/alerts', methods=['GET'])
@admin_required
def get_all_alerts():
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)
    pagination = Alert.query.order_by(Alert.created_at.desc()).paginate(page=page, per_page=per_page, error_out=False)
    return jsonify({
        'alerts': [a.to_dict() for a in pagination.items],
        'total': pagination.total,
        'page': page,
        'per_page': per_page,
        'pages': pagination.pages,
    }), 200


@admin_bp.route('/send-alert', methods=['POST'])
@admin_required
def send_alert():
    admin_id = int(get_jwt_identity())
    data = request.get_json()
    if not data or not data.get('user_id'):
        return jsonify({'error': '请指定目标用户'}), 400

    user = User.query.get(data['user_id'])
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    alert = Alert(
        user_id=data['user_id'],
        admin_id=admin_id,
        alert_type=data.get('alert_type', 'warning'),
        title=data.get('title', '驾驶安全提醒'),
        message=data.get('message', ''),
    )
    db.session.add(alert)
    db.session.commit()
    return jsonify({'message': '预警已发送', 'alert': alert.to_dict()}), 201


# ==================== 仪表盘 ====================



# ==================== 信用规则透明API（云端优化方案） ====================

@admin_bp.route('/credit/rules', methods=['GET'])
@admin_required
def get_credit_rules():
    """返回信用分扣分/加分规则及等级说明（规则透明）"""
    return jsonify({
        'penalties': {
            'phone': {'points': -5, 'description': '驾驶中使用手机'},
            'no_seatbelt': {'points': -3, 'description': '未系安全带'},
            'yawn': {'points': -2, 'description': '打哈欠（疲劳驾驶）'},
        },
        'rewards': {
            'daily_safe': {'points': 1, 'description': '当日零违规'},
            'weekly_clean': {'points': 3, 'description': '连续7天无违规'},
            'daily_active': {'points': 1, 'description': '每日设备上线'},
        },
        'levels': {
            'excellent': {'min': 90, 'label': '优秀', 'consequence': '优先派单、奖励'},
            'good': {'min': 70, 'label': '良好', 'consequence': '正常派单'},
            'fair': {'min': 50, 'label': '一般', 'consequence': '正常派单'},
            'warning': {'min': 40, 'label': '警告', 'consequence': '<=60系统发提醒，<=40暂停派单'},
            'blacklist': {'min': 30, 'label': '危险', 'consequence': '<=30自动拉黑，>=50自动解封'},
        },
        'recovery': {
            'daily_safe': '昨日零违规 +1分（上限100）',
            'weekly_clean': '连续7天无违规 +3分',
            'daily_active': '每日设备上线 +1分',
            'auto_unblock': '信用分回升至>=50自动解除黑名单',
        }
    }), 200


@admin_bp.route('/users/ranking', methods=['GET'])
@admin_required
def get_user_ranking():
    """用户信用排行（管理员端）"""
    drivers = User.query.filter_by(role='driver', is_active=1).order_by(User.credit_score.desc()).all()
    result = []
    for i, u in enumerate(drivers, 1):
        result.append({
            'rank': i,
            'id': u.id,
            'username': u.username,
            'real_name': u.real_name,
            'company_name': u.company.name if u.company else None,
            'license_plate': u.license_plate,
            'credit_score': u.credit_score,
            'total_trips': u.total_trips,
            'violation_count': u.violation_count,
            'blacklisted': u.blacklisted,
        })
    return jsonify({'ranking': result, 'total': len(result)}), 200


@admin_bp.route('/dashboard', methods=['GET'])
@admin_required
def get_dashboard():
    today = date.today()
    week_ago = today - timedelta(days=7)

    total_users = User.query.filter_by(role='driver', is_active=1).count()
    blacklisted_count = User.query.filter_by(role='driver', blacklisted=1).count()
    total_companies = Company.query.count()

    today_dangerous = db.session.query(func.sum(DailyStat.total_dangerous)).filter(DailyStat.stat_date == today).scalar() or 0
    today_detections = db.session.query(func.sum(DailyStat.total_detections)).filter(DailyStat.stat_date == today).scalar() or 0
    active_users_today = db.session.query(DailyStat.user_id).filter(DailyStat.stat_date == today).distinct().count()
    unread_alerts = Alert.query.filter_by(is_read=0).count()

    # 信用分布
    credit_distribution = {
        'excellent': User.query.filter(User.role == 'driver', User.credit_score >= 90, User.is_active == 1).count(),
        'good': User.query.filter(User.role == 'driver', User.credit_score >= 70, User.credit_score < 90, User.is_active == 1).count(),
        'fair': User.query.filter(User.role == 'driver', User.credit_score >= 50, User.credit_score < 70, User.is_active == 1).count(),
        'poor': User.query.filter(User.role == 'driver', User.credit_score >= 30, User.credit_score < 50, User.is_active == 1).count(),
        'critical': User.query.filter(User.role == 'driver', User.credit_score < 30, User.is_active == 1).count(),
    }

    average_credit = db.session.query(func.avg(User.credit_score)).filter(User.role == 'driver', User.is_active == 1).scalar() or 0

    # 近7天违规类别分布（饼图用）
    weekly_stats = db.session.query(
        func.sum(DailyStat.no_seatbelt_count).label('no_seatbelt'),
        func.sum(DailyStat.phone_count).label('phone'),
        func.sum(DailyStat.yawn_count).label('yawn'),
        func.sum(DailyStat.hands_off_count).label('hands_off'),
    ).filter(DailyStat.stat_date >= week_ago).first()

    # 各公司信用排行
    companies_rank = []
    companies = Company.query.all()
    for c in companies:
        avg = db.session.query(func.avg(User.credit_score)).filter(
            User.company_id == c.id, User.is_active == 1
        ).scalar() or 0
        driver_count = User.query.filter_by(company_id=c.id, role='driver', is_active=1).count()
        companies_rank.append({
            'id': c.id,
            'name': c.name,
            'avg_credit': round(float(avg), 1),
            'driver_count': driver_count,
        })

    # 今日危险行为趋势（柱状图用）- 按小时
    hourly_dangerous = {}
    for h in range(24):
        hourly_dangerous[str(h).zfill(2)] = 0

    today_records = DetectionRecord.query.filter(
        DetectionRecord.is_dangerous == 1,
        func.date(DetectionRecord.detected_at) == today
    ).all()
    for r in today_records:
        hour = r.detected_at.strftime('%H')
        if hour in hourly_dangerous:
            hourly_dangerous[hour] += 1

    return jsonify({
        'total_users': total_users,
        'blacklisted_count': blacklisted_count,
        'total_companies': total_companies,
        'today_dangerous': int(today_dangerous),
        'today_detections': int(today_detections),
        'active_users_today': active_users_today,
        'unread_alerts': unread_alerts,
        'average_credit': round(float(average_credit), 1),
        'credit_distribution': credit_distribution,
        'companies_rank': companies_rank,
        'hourly_dangerous': hourly_dangerous,
        'weekly_category_distribution': {
            'no_seatbelt': int(weekly_stats.no_seatbelt or 0),
            'phone': int(weekly_stats.phone or 0),
            'yawn': int(weekly_stats.yawn or 0),
            'hands_off': int(weekly_stats.hands_off or 0),
        },
        'score_thresholds': {
            'excellent': {'min': 90, 'label': '优秀'},
            'good': {'min': 70, 'label': '良好'},
            'fair': {'min': 50, 'label': '一般'},
            'poor': {'min': 30, 'label': '较差'},
            'critical': {'min': 0, 'label': '危险'},
        }
    }), 200
