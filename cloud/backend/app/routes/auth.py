from flask import Blueprint, request, jsonify
from flask_jwt_extended import create_access_token, jwt_required, get_jwt_identity
from werkzeug.security import generate_password_hash, check_password_hash
from app import db
from app.models import User

auth_bp = Blueprint('auth', __name__)


@auth_bp.route('/login', methods=['POST'])
def login():
    data = request.get_json()
    if not data:
        return jsonify({'error': '请提供登录信息'}), 400

    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({'error': '用户名和密码不能为空'}), 400

    user = User.query.filter_by(username=username).first()
    if not user or not check_password_hash(user.password_hash, password):
        return jsonify({'error': '用户名或密码错误'}), 401

    if not user.is_active:
        return jsonify({'error': '账户已被禁用'}), 403

    if user.blacklisted and user.role == 'driver':
        return jsonify({'error': '您的账号已被加入黑名单，请联系管理员处理'}), 403

    access_token = create_access_token(
        identity=str(user.id),
        additional_claims={'role': user.role, 'username': user.username}
    )

    return jsonify({'token': access_token, 'user': user.to_dict()}), 200


@auth_bp.route('/ranking', methods=['GET'])
@jwt_required()
def get_my_ranking():
    """当前用户的信用排名"""
    user_id = int(get_jwt_identity())

    total = User.query.filter_by(role='driver', is_active=1).count()
    user = User.query.get(user_id)
    if not user or user.role != 'driver':
        return jsonify({'rank': None, 'total': total}), 200

    # Count users with higher credit score
    higher = User.query.filter(
        User.role == 'driver',
        User.is_active == 1,
        User.credit_score > user.credit_score
    ).count()

    rank = higher + 1
    return jsonify({'rank': rank, 'total': total, 'credit_score': user.credit_score}), 200


@auth_bp.route('/credit/rules', methods=['GET'])
@jwt_required()
def get_credit_rules():
    """信用规则透明接口（司机可访问）"""
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
            'excellent': {'min': 90, 'label': '优秀', 'consequence': '优先派单、奖励', 'color': 'green'},
            'good': {'min': 70, 'label': '良好', 'consequence': '正常派单', 'color': 'blue'},
            'fair': {'min': 50, 'label': '一般', 'consequence': '正常派单', 'color': 'yellow'},
            'warning': {'min': 40, 'label': '警告', 'consequence': '<=60系统发提醒，<=40暂停派单', 'color': 'orange'},
            'blacklist': {'min': 30, 'label': '危险', 'consequence': '<=30自动拉黑，>=50自动解封', 'color': 'red'},
        },
        'recovery': {
            'daily_safe': '昨日零违规 +1分（上限100）',
            'weekly_clean': '连续7天无违规 +3分',
            'daily_active': '每日设备上线 +1分',
            'auto_unblock': '信用分回升至>=50自动解除黑名单',
        }
    }), 200


@auth_bp.route('/register', methods=['POST'])
def register():
    data = request.get_json()
    if not data:
        return jsonify({'error': '请提供注册信息'}), 400

    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({'error': '用户名和密码不能为空'}), 400

    if User.query.filter_by(username=username).first():
        return jsonify({'error': '用户名已存在'}), 409

    user = User(
        username=username,
        password_hash=generate_password_hash(password),
        role=data.get('role', 'user'),
        real_name=data.get('real_name'),
        email=data.get('email'),
        device_id=data.get('device_id'),
        credit_score=100,  # 新用户初始信用分100
        company_id=data.get('company_id'),
        license_plate=data.get('license_plate'),
    )
    db.session.add(user)
    db.session.commit()

    return jsonify({'message': '注册成功', 'user': user.to_dict()}), 201


@auth_bp.route('/credit/history', methods=['GET'])
@jwt_required()
def get_credit_history():
    """当前用户的信用分变更记录"""
    user_id = int(get_jwt_identity())
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)

    from app.models import CreditLog
    query = CreditLog.query.filter_by(user_id=user_id).order_by(CreditLog.created_at.desc())
    pagination = query.paginate(page=page, per_page=per_page, error_out=False)

    return jsonify({
        'logs': [l.to_dict() for l in pagination.items],
        'total': pagination.total,
        'page': page,
        'per_page': per_page,
        'pages': pagination.pages,
    }), 200


@auth_bp.route('/profile', methods=['GET'])
@jwt_required()
def get_profile():
    user_id = int(get_jwt_identity())
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404
    return jsonify({'user': user.to_dict()}), 200


@auth_bp.route('/profile', methods=['PUT'])
@jwt_required()
def update_profile():
    user_id = int(get_jwt_identity())
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404

    data = request.get_json()
    if data.get('real_name'):
        user.real_name = data['real_name']
    if data.get('email'):
        user.email = data['email']
    if data.get('device_id'):
        user.device_id = data['device_id']
    if data.get('license_plate'):
        user.license_plate = data['license_plate']

    db.session.commit()
    return jsonify({'message': '更新成功', 'user': user.to_dict()}), 200