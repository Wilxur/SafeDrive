from flask import Blueprint, request, jsonify
from flask_jwt_extended import jwt_required, get_jwt_identity
from app import db
from app.models import DetectionRecord, DailyStat
from datetime import datetime, date, timedelta
from sqlalchemy import func

detection_bp = Blueprint('detection', __name__)

# 优化方案: 6类检测，仅3类危险
DANGEROUS_CATEGORIES = ['no_seatbelt', 'phone', 'yawn', 'hands_off']

# 加分规则（与admin.py保持一致引用，避免循环导入）
CREDIT_REWARDS = {
    'daily_safe': 1,      # 当日零违规 +1
    'weekly_clean': 3,    # 连续7天无违规 +3
    'daily_active': 1,    # 每日设备上线 +1
}


@detection_bp.route('/upload', methods=['POST'])
@jwt_required()
def upload_detection():
    """开发板上传检测数据（优化版）"""
    user_id = int(get_jwt_identity())
    data = request.get_json()

    if not data:
        return jsonify({'error': '请提供检测数据'}), 400

    category = data.get('category')

    # 优化方案: 6类对齐
    valid = ['yawn', 'seatbelt', 'no_seatbelt', 'phone', 'steering_wheel', 'hand', 'hands_off']
    if category not in valid:
        return jsonify({'error': '无效的检测类别'}), 400

    detected_at_str = data.get('detected_at')
    if detected_at_str:
        try:
            detected_at = datetime.fromisoformat(detected_at_str)
        except ValueError:
            detected_at = datetime.now()
    else:
        detected_at = datetime.now()

    is_dangerous = 1 if category in DANGEROUS_CATEGORIES else 0
    today = detected_at.date()

    from app.models import User
    user = User.query.get(user_id)

    # ----- 每日首次检测计为一次出车 -----
    today_first = DetectionRecord.query.filter(
        DetectionRecord.user_id == user_id,
        func.date(DetectionRecord.detected_at) == today
    ).first()
    is_first_today = (today_first is None)
    if is_first_today and user:
        user.total_trips = (user.total_trips or 0) + 1

    record = DetectionRecord(
        user_id=user_id,
        device_id=data.get('device_id'),
        category=category,
        confidence=data.get('confidence'),
        image_url=data.get('image_url'),
        is_dangerous=is_dangerous,
        detected_at=detected_at,
    )
    db.session.add(record)

    stat = DailyStat.query.filter_by(user_id=user_id, stat_date=today).first()
    if not stat:
        stat = DailyStat(user_id=user_id, stat_date=today)
        db.session.add(stat)

    # 处理旧8类字段（兼容不影响）
    cat_field = category + '_count'
    if hasattr(stat, cat_field):
        setattr(stat, cat_field, (getattr(stat, cat_field) or 0) + 1)
    stat.total_detections = (stat.total_detections or 0) + 1
    if is_dangerous:
        stat.total_dangerous = (stat.total_dangerous or 0) + 1

        # ---------- 信用分自动扣减（30s内同种只扣一次）----------
    if is_dangerous and user:
        from app.models import CreditLog
        from app.routes.admin import CREDIT_PENALTIES

        # 30s内同种违规只扣一次分
        thirty_sec_ago = datetime.now() - timedelta(seconds=30)
        recent_penalty = CreditLog.query.filter(
            CreditLog.user_id == user_id,
            CreditLog.reason.like(f'检测到{category}，扣除%'),
            CreditLog.created_at >= thirty_sec_ago
        ).first()

        if recent_penalty:
            # 30s内已扣过同种，跳过扣分
            pass
        else:
            penalty = CREDIT_PENALTIES.get(category, 0)
            if penalty > 0:
                old_score = user.credit_score
                new_score = max(0, old_score - penalty)
                user.credit_score = new_score
                user.violation_count = (user.violation_count or 0) + 1
                stat.credit_deducted = (stat.credit_deducted or 0) + penalty

                log = CreditLog(
                    user_id=user.id,
                    change=-penalty,
                    score_after=new_score,
                    reason=f'检测到{category}，扣除{penalty}信用分',
                )
                db.session.add(log)

                # ---------- 分级预警阈值 ----------
                from app.models import Alert

                # <=60 黄色提醒
                if new_score <= 60 and old_score > 60:
                    existing = Alert.query.filter_by(
                        user_id=user.id,
                        alert_type='warning',
                        title='信用分进入预警区',
                    ).filter(
                        func.date(Alert.created_at) == today
                    ).first()
                    if not existing:
                        alert = Alert(
                            user_id=user.id,
                            admin_id=1,
                            alert_type='warning',
                            title='信用分进入预警区',
                            message=f'您的信用分已降至{new_score}分，进入预警区（<=60），请注意驾驶行为。',
                        )
                        db.session.add(alert)

                # <=40 橙色警告 + 标记暂停派单
                if new_score <= 40:
                    existing = Alert.query.filter_by(
                        user_id=user.id,
                        alert_type='urgent',
                        title='信用分过低，已限制出车',
                    ).filter(
                        func.date(Alert.created_at) == today
                    ).first()
                    if not existing:
                        alert = Alert(
                            user_id=user.id,
                            admin_id=1,
                            alert_type='urgent',
                            title='信用分过低，已限制出车',
                            message=f'您的信用分已降至{new_score}分，过低！已被限制出车，请联系管理员。',
                        )
                        db.session.add(alert)

                # <=30 自动拉黑
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

# ---------- 信用恢复：检查昨日安全驾驶 ----------
    if user:
        check_and_reward_safe_driving(user, today)

    db.session.commit()
    return jsonify({
        'message': '检测数据已记录',
        'record': record.to_dict(),
        'credit_score': user.credit_score if user else 100,
        'blacklisted': user.blacklisted if user else 0
    }), 201


def check_and_reward_safe_driving(user, today):
    """检查昨日安全驾驶情况并奖励"""
    from app.models import CreditLog, Alert

    yesterday = today - timedelta(days=1)

    # 昨日检测记录
    yesterday_dangerous = DetectionRecord.query.filter_by(
        user_id=user.id, is_dangerous=1
    ).filter(
        func.date(DetectionRecord.detected_at) == yesterday
    ).count()

    # 1. 昨日零违规 +1
    if yesterday_dangerous == 0:
        already_rewarded = CreditLog.query.filter(
            CreditLog.user_id == user.id,
            CreditLog.reason == '昨日安全驾驶，+1分',
            func.date(CreditLog.created_at) == today
        ).first()
        if not already_rewarded and user.credit_score < 100:
            add_credit(user, CREDIT_REWARDS['daily_safe'], '昨日安全驾驶，+1分')

    # 2. 检查是否有昨日设备上线记录
    yesterday_has_activity = DetectionRecord.query.filter_by(
        user_id=user.id
    ).filter(
        func.date(DetectionRecord.detected_at) == yesterday
    ).first()

    if yesterday_has_activity:
        already_rewarded_active = CreditLog.query.filter(
            CreditLog.user_id == user.id,
            CreditLog.reason == '每日设备上线，+1分',
            func.date(CreditLog.created_at) == today
        ).first()
        if not already_rewarded_active and user.credit_score < 100:
            add_credit(user, CREDIT_REWARDS['daily_active'], '每日设备上线，+1分')

    # 3. 连续7天无违规 +3
    week_ago = today - timedelta(days=7)
    week_dangerous = DetectionRecord.query.filter_by(
        user_id=user.id, is_dangerous=1
    ).filter(
        func.date(DetectionRecord.detected_at) >= week_ago,
        func.date(DetectionRecord.detected_at) < today
    ).count()

    if week_dangerous == 0:
        already_rewarded_week = CreditLog.query.filter(
            CreditLog.user_id == user.id,
            CreditLog.reason == '连续7天无违规，+3分',
            func.date(CreditLog.created_at) == today
        ).first()
        if not already_rewarded_week and user.credit_score <= 97:
            add_credit(user, CREDIT_REWARDS['weekly_clean'], '连续7天无违规，+3分')

    # 4. 自动解封：≥50
    if user.credit_score >= 50 and user.blacklisted:
        user.blacklisted = 0
        already_unblocked = CreditLog.query.filter(
            CreditLog.user_id == user.id,
            CreditLog.reason == '信用分回升至安全线，自动解除黑名单',
            func.date(CreditLog.created_at) == today
        ).first()
        if not already_unblocked:
            log = CreditLog(
                user_id=user.id,
                change=0,
                score_after=user.credit_score,
                reason='信用分回升至安全线，自动解除黑名单'
            )
            db.session.add(log)
            alert = Alert(
                user_id=user.id,
                admin_id=1,
                alert_type='system',
                title='恭喜！黑名单已自动解除',
                message=f'您的信用分已回升至{user.credit_score}分，达到安全线，黑名单已自动解除。',
            )
            db.session.add(alert)


def add_credit(user, points, reason):
    """安全加分（不超过100）"""
    from app.models import CreditLog
    old_score = user.credit_score
    new_score = min(100, old_score + points)
    actual_change = new_score - old_score
    if actual_change <= 0:
        return
    user.credit_score = new_score
    log = CreditLog(
        user_id=user.id,
        change=actual_change,
        score_after=new_score,
        reason=reason,
    )
    db.session.add(log)


@detection_bp.route('/today', methods=['GET'])
@jwt_required()
def get_today():
    """今日检测汇总"""
    user_id = int(get_jwt_identity())
    today = date.today()
    from app.models import User, CreditLog

    records = DetectionRecord.query.filter(
        DetectionRecord.user_id == user_id,
        func.date(DetectionRecord.detected_at) == today
    ).order_by(DetectionRecord.detected_at.desc()).all()

    stat = DailyStat.query.filter_by(user_id=user_id, stat_date=today).first()
    user = User.query.get(user_id)

    return jsonify({
        'total_detections': len(records),
        'dangerous_count': sum(1 for r in records if r.is_dangerous),
        'safe_count': sum(1 for r in records if not r.is_dangerous),
        'records': [r.to_dict() for r in records[:10]],
        'daily_stat': stat.to_dict() if stat else None,
        'credit_score': user.credit_score if user else 100,
    }), 200


@detection_bp.route('/records', methods=['GET'])
@jwt_required()
def get_records():
    user_id = int(get_jwt_identity())
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)
    category = request.args.get('category')
    start_date = request.args.get('start_date')
    end_date = request.args.get('end_date')
    dangerous_only = request.args.get('dangerous_only', 0, type=int)

    query = DetectionRecord.query.filter_by(user_id=user_id)
    if category:
        query = query.filter_by(category=category)
    if start_date:
        query = query.filter(DetectionRecord.detected_at >= start_date)
    if end_date:
        query = query.filter(DetectionRecord.detected_at <= end_date + ' 23:59:59')
    if dangerous_only:
        query = query.filter_by(is_dangerous=1)

    query = query.order_by(DetectionRecord.detected_at.desc())
    pagination = query.paginate(page=page, per_page=per_page, error_out=False)

    return jsonify({
        'records': [r.to_dict() for r in pagination.items],
        'total': pagination.total,
        'page': page,
        'per_page': per_page,
        'pages': pagination.pages,
    }), 200


@detection_bp.route('/stats', methods=['GET'])
@jwt_required()
def get_stats():
    user_id = int(get_jwt_identity())
    days = request.args.get('days', 7, type=int)
    start_date = date.today() - timedelta(days=days - 1)

    stats = DailyStat.query.filter(
        DailyStat.user_id == user_id,
        DailyStat.stat_date >= start_date
    ).order_by(DailyStat.stat_date.asc()).all()

    summary = db.session.query(
        func.sum(DailyStat.total_dangerous).label('total_dangerous'),
        func.sum(DailyStat.total_detections).label('total_detections'),
        func.sum(DailyStat.phone_count).label('phone_count'),
        func.sum(DailyStat.yawn_count).label('yawn_count'),
        func.sum(DailyStat.no_seatbelt_count).label('no_seatbelt_count'),
        func.sum(DailyStat.hand_count).label('hand_count'),
        func.sum(DailyStat.hands_off_count).label('hands_off_count'),
    ).filter(
        DailyStat.user_id == user_id,
        DailyStat.stat_date >= start_date
    ).first()

    from app.models import User
    user = User.query.get(user_id)

    return jsonify({
        'daily_stats': [s.to_dict() for s in stats],
        'credit_score': user.credit_score if user else 100,
        'summary': {
        'total_dangerous': int(summary.total_dangerous or 0),
        'total_detections': int(summary.total_detections or 0),
        'phone_count': int(summary.phone_count or 0),
        'yawn_count': int(summary.yawn_count or 0),
        'no_seatbelt_count': int(summary.no_seatbelt_count or 0),
        'hand_count': int(summary.hand_count or 0),
        }
    }), 200


@detection_bp.route('/realtime', methods=['GET'])
@jwt_required()
def get_realtime():
    user_id = int(get_jwt_identity())
    latest = DetectionRecord.query.filter_by(user_id=user_id).order_by(DetectionRecord.detected_at.desc()).first()
    if not latest:
        return jsonify({'latest': None}), 200

    from app.models import User
    user = User.query.get(user_id)

    return jsonify({
        'latest': latest.to_dict(),
        'credit_score': user.credit_score if user else 100,
        'blacklisted': user.blacklisted if user else 0,
    }), 200
