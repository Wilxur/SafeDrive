#!/bin/bash
# ============================================
# 货车信用管理平台 - 服务器部署更新脚本
# 在 SSH 登录服务器后运行
# 用法: sudo bash deploy_update.sh
# ============================================

set -e

APP_DIR="/opt/fatigue-driving"
BACKEND_DIR="${APP_DIR}/backend"

echo "========================================"
echo "  货车信用管理平台 - 更新脚本"
echo "========================================"

# 1. 导入新数据库表结构
echo ""
echo "[1/4] 更新数据库表结构..."
echo "请输入 MySQL root 密码..."
mysql -u root -p car_mo < ${APP_DIR}/database/schema.sql
echo "数据库表结构已更新"

# 2. 检查并安装 Python 依赖
echo ""
echo "[2/4] 检查 Python 依赖..."
source "${BACKEND_DIR}/venv/bin/activate"
pip install -r "${BACKEND_DIR}/requirements.txt" --quiet
deactivate
echo "依赖检查完成"

# 3. 设置文件权限
echo ""
echo "[3/4] 设置文件权限..."
chown -R www-data:www-data ${APP_DIR} 2>/dev/null || chown -R nginx:nginx ${APP_DIR} 2>/dev/null || true
chmod -R 755 ${APP_DIR}
echo "权限设置完成"

# 4. 重启服务
echo ""
echo "[4/4] 重启服务..."
systemctl daemon-reload
systemctl restart fatigue-driving
systemctl reload nginx 2>/dev/null || systemctl restart nginx

echo ""
echo "========================================"
echo "  更新完成！"
echo "========================================"
echo ""
echo "服务状态检查："
systemctl status fatigue-driving --no-pager -l | head -15
echo ""
echo "访问 http://服务器IP 查看效果"
echo ""
echo "默认账户："
echo "  管理员: admin / admin123"
echo "  测试司机: test / test123"
echo ""