@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] ERROR: Please run as Administrator.
    pause
    exit /b
)

echo Adding IP alias for 45.58.9.21...
netsh interface ipv4 add address "Loopback Pseudo-Interface 1" 45.58.9.21 255.255.255.255

echo Adding IP alias for 45.58.9.172...
netsh interface ipv4 add address "Loopback Pseudo-Interface 1" 45.58.9.172 255.255.255.255

echo Setting up portproxy 45.58.9.21:1818 -> 127.0.0.1:8080...
netsh interface portproxy add v4tov4 listenaddress=45.58.9.21 listenport=1818 connectaddress=127.0.0.1 connectport=8080

echo Setting up portproxy 45.58.9.172:1818 -> 127.0.0.1:8081...
netsh interface portproxy add v4tov4 listenaddress=45.58.9.172 listenport=1818 connectaddress=127.0.0.1 connectport=8081

echo [+] Both tunnels configured!
pause