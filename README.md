# Shiltz
Seal Online Server Emulator

## Building on Linux

Install CMake, Ninja, a C++20 compiler, and vcpkg, then point `VCPKG_ROOT` at the vcpkg checkout:

```sh
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset linux-debug
cmake --build out/build/linux-debug --target login game
```

The server executables are written to `out/build/linux-debug/src/servers/login/login` and
`out/build/linux-debug/src/servers/game/game`.

For an optimized build, use the `linux-release` preset instead.
