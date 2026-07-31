@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Please run as Administrator.
    pause
    exit /b
)

echo Removing portproxy rules...

netsh interface portproxy delete v4tov4 listenaddress=45.58.9.21 listenport=1818
netsh interface portproxy delete v4tov4 listenaddress=45.58.9.172 listenport=1818

echo Removing IP aliases...

netsh interface ipv4 delete address "Loopback Pseudo-Interface 1" addr=45.58.9.21
netsh interface ipv4 delete address "Loopback Pseudo-Interface 1" addr=45.58.9.172

echo [+] Tunnels removed!

echo.
echo Current Portproxy Rules:
netsh interface portproxy show all

pause