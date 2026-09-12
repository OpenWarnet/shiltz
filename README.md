# Shiltz

Shiltz is an experimental, from-scratch C++ server emulator for **Seal Online**. It reimplements the game's binary network protocol and currently provides separate login and game servers.

> [!WARNING]
> This project is under active development and is intended for local development and protocol testing. It is not production-ready.

## Get the source

```sh
git clone https://github.com/OpenWarnet/shiltz.git
cd shiltz
```

## Build on Linux

The commands below target Ubuntu and Debian. On another distribution, install the equivalent C++ compiler, CMake, Ninja, Git, and vcpkg prerequisites. The repository's presets require CMake 3.21 or newer.

### 1. Install the build tools

```sh
sudo apt update
sudo apt install -y build-essential cmake ninja-build gcc g++ git curl zip unzip tar pkg-config
```

### 2. Install vcpkg

```sh
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

`VCPKG_ROOT` must be set in every shell used to configure the project. Add the export to your shell profile if you want it to persist.

### 3. Configure and build

From the Shiltz repository root:

```sh
cmake --preset linux-debug
cmake --build out/build/linux-debug --target login game --parallel
```

CMake uses the repository's `vcpkg.json` manifest to install the required Crypto++, SQLite, and Boost.Asio packages.

The executables are created at:

- `out/build/linux-debug/src/servers/login/login`
- `out/build/linux-debug/src/servers/game/game`

For an optimized build, replace `linux-debug` with `linux-release` in both commands.

## Build on Windows

### 1. Install the prerequisites

Install the following:

1. [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (the Community edition is sufficient).
2. In Visual Studio Installer, select the **Desktop development with C++** workload.
3. In the workload's installation details, make sure these components are enabled:

   - MSVC v143 C++ build tools
   - A Windows 10 or Windows 11 SDK
   - C++ CMake tools for Windows
   - vcpkg package manager

4. Install [Git for Windows](https://git-scm.com/download/win).

### 2. Open a Visual Studio developer shell

Open **Developer PowerShell for VS 2022** or **x64 Native Tools Command Prompt for VS 2022** from the Start menu. The Windows presets use Visual Studio's compiler and bundled vcpkg installation, so a regular terminal may not have the required `VSINSTALLDIR` environment variable.

### 3. Configure and build

From the Shiltz repository root:

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug --target login game --parallel
```

The executables are created at:

- `out\build\x64-debug\src\servers\login\login.exe`
- `out\build\x64-debug\src\servers\game\game.exe`

Use the `x64-release` preset for an optimized build. The repository also provides `x86-debug` and `x86-release` presets when a 32-bit build is needed.

## Run the servers

Run each server in its own terminal from the repository root. Start the login server first and wait for `Login listening on port 8080...`, then start the game server and wait for `Game listening on port 8081...`.

### Linux

Terminal 1:

```sh
./out/build/linux-debug/src/servers/login/login
```

Terminal 2:

```sh
./out/build/linux-debug/src/servers/game/game
```

### Windows

Terminal 1:

```powershell
.\out\build\x64-debug\src\servers\login\login.exe
```

Terminal 2:

```powershell
.\out\build\x64-debug\src\servers\game\game.exe
```

On first startup, database migrations run automatically and create `db/login.sqlite3`. Decrypted packet logs are written to `logs/`.

## Test with the official client on Windows

The official client is not included in this repository. With both Shiltz server processes running:

1. Open PowerShell or Command Prompt **as Administrator**, change to the repository root, and configure the loopback aliases and port forwarding:

   ```powershell
   .\scripts\Start.bat
   ```

2. Open the official Seal Online client as usual.

3. Log in with any new username and password. The development server automatically creates the account on its first login. If that username already exists, use the same password that was used when it was created.

4. Select the server and create or select a character to exercise the handoff from the login server to the game server.

Use throwaway credentials: development passwords are currently stored as plain text in the local SQLite database and must not be reused elsewhere.

When you finish testing, remove the loopback aliases and forwarding rules from an elevated terminal:

```powershell
.\scripts\Stop.bat
```

The bundled loopback scripts are Windows-only. A Linux build can host the server, but connecting the official Windows client from another machine requires equivalent network routing that is not configured by this repository.

## Troubleshooting

- **CMake cannot find `cl.exe` or `VSINSTALLDIR`:** run the build from a Visual Studio 2022 developer shell and verify that the C++ workload is installed.
- **CMake rejects `CMakePresets.json` on Linux:** install CMake 3.21 or newer.
- **CMake cannot find the vcpkg toolchain on Linux:** confirm that `VCPKG_ROOT` points to the bootstrapped vcpkg checkout before running the configure command.
- **The official client cannot connect:** confirm that both servers display their listening messages and that `scripts\Start.bat` was run as Administrator.
- **The loopback setup already exists or is stale:** run `scripts\Stop.bat` as Administrator, then run `scripts\Start.bat` again.
- **A port is already in use:** stop the process using TCP port `8080` or `8081`, then restart the corresponding server.
