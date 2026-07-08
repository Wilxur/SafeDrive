# ============================================
# 疲劳驾驶检测系统 - Windows 部署上传脚本
# 使用 scp 将代码上传到 Linux 服务器
# 用法: powershell -File deploy_upload.ps1
# ============================================

$server = "root@8.133.22.44"
$port = 22
$localRoot = "D:\疲劳驾驶"
$remoteRoot = "/opt/fatigue-driving"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  货车信用平台 - 部署上传脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 先构建前端
Write-Host "[1/4] 构建前端..." -ForegroundColor Yellow
Set-Location "$localRoot\frontend"
npx.cmd vite build 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "前端构建失败！请检查错误" -ForegroundColor Red
    exit 1
}
Write-Host "前端构建完成" -ForegroundColor Green
Write-Host ""

# 2. 上传后端
Write-Host "[2/4] 上传后端代码..." -ForegroundColor Yellow
scp -P $port -r "$localRoot\backend" "${server}:${remoteRoot}/"
if ($LASTEXITCODE -ne 0) { Write-Host "后端上传失败" -ForegroundColor Red; exit 1 }
Write-Host "后端上传完成" -ForegroundColor Green
Write-Host ""

# 3. 上传前端编译结果
Write-Host "[3/4] 上传前端静态文件..." -ForegroundColor Yellow
scp -P $port -r "$localRoot\frontend\dist" "${server}:${remoteRoot}/frontend/"
if ($LASTEXITCODE -ne 0) { Write-Host "前端上传失败" -ForegroundColor Red; exit 1 }
Write-Host "前端上传完成" -ForegroundColor Green
Write-Host ""

# 4. 上传数据库脚本和部署配置
Write-Host "[4/4] 上传数据库脚本和部署配置..." -ForegroundColor Yellow
scp -P $port -r "$localRoot\database" "${server}:${remoteRoot}/"
scp -P $port -r "$localRoot\deploy" "${server}:${remoteRoot}/"
if ($LASTEXITCODE -ne 0) { Write-Host "配置文件上传失败" -ForegroundColor Red; exit 1 }
Write-Host "配置文件上传完成" -ForegroundColor Green
Write-Host ""

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  上传完成！请登录服务器执行后续操作" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "SSH 登录: ssh -p 22 root@8.133.22.44" -ForegroundColor White
Write-Host ""
Write-Host "然后在服务器上依次执行：" -ForegroundColor Yellow
Write-Host "  1. 导入数据库表结构：" -ForegroundColor Gray
Write-Host "     mysql -u root -p car_mo < /opt/fatigue-driving/database/schema.sql" -ForegroundColor White
Write-Host ""
Write-Host "  2. 重新安装依赖（如需）：" -ForegroundColor Gray
Write-Host "     cd /opt/fatigue-driving/backend && source venv/bin/activate && pip install -r requirements.txt && deactivate" -ForegroundColor White
Write-Host ""
Write-Host "  3. 重启后端服务：" -ForegroundColor Gray
Write-Host "     sudo systemctl restart fatigue-driving" -ForegroundColor White
Write-Host ""
Write-Host "  4. 重启 Nginx：" -ForegroundColor Gray
Write-Host "     sudo systemctl reload nginx" -ForegroundColor White
Write-Host ""
Write-Host "  5. 查看运行状态：" -ForegroundColor Gray
Write-Host "     journalctl -u fatigue-driving -n 20 --no-pager" -ForegroundColor White
Write-Host ""