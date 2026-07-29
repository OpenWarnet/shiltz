@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Please run as Administrator.
    pause
    exit /b
)

echo Removing portproxy rule for port 1818...
netsh interface portproxy delete v4tov4 listenport=1818 listenaddress=0.0.0.0
netsh interface portproxy delete v4tov4 listenport=1818 listenaddress=45.58.9.21

echo Current Portproxy Rules:
netsh interface portproxy show all

pause