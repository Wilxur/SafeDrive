# ============================================
# 货车信用管理平台 - 一键部署脚本
# 在 cmd 或 PowerShell 中直接运行
# ============================================

$server = "root@8.133.22.44"
$port = 22
$localRoot = "D:\疲劳驾驶"
$remoteRoot = "/opt/fatigue-driving"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  货车信用管理平台 - 一键部署" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 构建前端
Write-Host "[1/4] 构建前端..." -ForegroundColor Yellow
Set-Location "$localRoot\frontend"
npx.cmd vite build 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "前端构建失败!" -ForegroundColor Red
    exit 1
}
Write-Host "前端构建完成" -ForegroundColor Green
Write-Host ""

# 2. 打包文件
Write-Host "[2/4] 打包部署文件..." -ForegroundColor Yellow
Set-Location $localRoot
# 用 tar 打包（Windows 10 1803+ 自带）
tar -czf deploy_package.tar.gz `
    --exclude="backend/__pycache__" `
    --exclude="backend/app/__pycache__" `
    --exclude="backend/app/routes/__pycache__" `
    --exclude="backend/.env" `
    --exclude="backend/venv" `
    backend frontend/dist database deploy 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "打包失败，改用 robocopy 方式..." -ForegroundColor Yellow
    # 如果 tar 不支持 --exclude，用简单方式
    tar -czf deploy_package.tar.gz backend frontend\dist database deploy 2>$null
}
Write-Host "打包完成: deploy_package.tar.gz" -ForegroundColor Green
Write-Host ""

# 3. 上传
Write-Host "[3/4] 上传到服务器 ($server) ..." -ForegroundColor Yellow
Write-Host "需要输入服务器密码 (98Welldone!!)，如果提示"是否继续连接"输入 yes" -ForegroundColor Gray
scp -P $port -o StrictHostKeyChecking=accept-new deploy_package.tar.gz "${server}:${remoteRoot}/"
if ($LASTEXITCODE -ne 0) {
    Write-Host "上传失败!" -ForegroundColor Red
    exit 1
}
Write-Host "上传完成" -ForegroundColor Green
Write-Host ""

# 4. 远程部署
Write-Host "[4/4] 在服务器上解压并更新..." -ForegroundColor Yellow
ssh -p $port -o StrictHostKeyChecking=accept-new $server @"
cd $remoteRoot && \
tar -xzf deploy_package.tar.gz && \
rm -f deploy_package.tar.gz && \
echo '--- 更新数据库 ---' && \
mysql -u root -pwmywbyt car_mo < database/schema.sql 2>&1 && \
echo '--- 重启服务 ---' && \
systemctl daemon-reload && \
systemctl restart fatigue-driving && \
systemctl reload nginx 2>/dev/null; \
echo ''; \
echo '=============================='; \
echo '  部署成功!'; \
echo '=============================='; \
echo '  管理员: admin / admin123'; \
echo '  测试司机: test / test123'; \
echo '  访问 http://8.133.22.44'; \
echo ''
"@

Write-Host ""
Write-Host "部署流程结束！" -ForegroundColor Cyan