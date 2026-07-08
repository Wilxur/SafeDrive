from flask import Blueprint, request, jsonify
from flask_jwt_extended import jwt_required, get_jwt_identity
from app import db
from app.models import Alert

alerts_bp = Blueprint('alerts', __name__)


@alerts_bp.route('/my', methods=['GET'])
@jwt_required()
def get_my_alerts():
    user_id = int(get_jwt_identity())
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)
    unread_only = request.args.get('unread_only', 0, type=int)

    query = Alert.query.filter_by(user_id=user_id)
    if unread_only:
        query = query.filter_by(is_read=0)

    query = query.order_by(Alert.created_at.desc())
    pagination = query.paginate(page=page, per_page=per_page, error_out=False)

    return jsonify({
        'alerts': [a.to_dict() for a in pagination.items],
        'total': pagination.total,
        'page': page,
        'per_page': per_page,
        'pages': pagination.pages,
        'unread_count': Alert.query.filter_by(user_id=user_id, is_read=0).count()
    }), 200


@alerts_bp.route('/<int:alert_id>/read', methods=['PUT'])
@jwt_required()
def mark_as_read(alert_id):
    user_id = int(get_jwt_identity())
    alert = Alert.query.filter_by(id=alert_id, user_id=user_id).first()
    if not alert:
        return jsonify({'error': '预警不存在'}), 404
    alert.is_read = 1
    db.session.commit()
    return jsonify({'message': '已标记为已读', 'alert': alert.to_dict()}), 200


@alerts_bp.route('/read-all', methods=['PUT'])
@jwt_required()
def mark_all_as_read():
    user_id = int(get_jwt_identity())
    Alert.query.filter_by(user_id=user_id, is_read=0).update({'is_read': 1})
    db.session.commit()
    return jsonify({'message': '已全部标记为已读'}), 200
