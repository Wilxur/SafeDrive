@echo off
chcp 65001 >nul
title Deploy Script

echo ========================================
echo   Truck Credit Platform - Deploy
echo ========================================
echo.

set APP_DIR=D:\疲劳驾驶

echo [1/4] Building frontend...
cd /d "%APP_DIR%\frontend"
call npx.cmd vite build
if %errorlevel% neq 0 (
    echo Frontend build failed!
    pause
    exit /b 1
)
echo Build complete
echo.

echo [2/4] Packaging files...
cd /d "%APP_DIR%"
if exist deploy_package.tar.gz del deploy_package.tar.gz
tar -czf deploy_package.tar.gz backend frontend\dist database deploy 2>nul
if %errorlevel% neq 0 (
    echo Package failed!
    pause
    exit /b 1
)
echo Package complete
echo.

echo [3/4] Uploading to 8.133.22.44 ...
echo Password: 98Welldone!!
scp -P 22 -o StrictHostKeyChecking=accept-new deploy_package.tar.gz root@8.133.22.44:/opt/fatigue-driving/
if %errorlevel% neq 0 (
    echo Upload failed!
    pause
    exit /b 1
)
echo Upload complete
echo.

echo [4/4] Deploying on server...
ssh -p 22 -o StrictHostKeyChecking=accept-new root@8.133.22.44 "cd /opt/fatigue-driving && tar -xzf deploy_package.tar.gz && rm -f deploy_package.tar.gz && echo --- Run migration --- && cd backend && source venv/bin/activate && python migrate_db.py && echo --- Restart services --- && systemctl daemon-reload && nohup systemctl restart fatigue-driving >/dev/null 2>&1 & nohup systemctl reload nginx >/dev/null 2>&1 & echo. && echo ============================== && echo  Deploy OK! && echo ============================== && echo  Admin: admin / admin123 && echo  Driver: test / test123 && echo  URL: http://8.133.22.44"
echo.
pause