# Codex Buddy for Windows

A native Windows tray companion for AI Passport. Supports local Codex task synchronization over USB serial and BLE (Nordic UART). No subtitles or keyboard monitoring.

See [中文使用说明](README.zh_CN.md) for setup, permissions, privacy, troubleshooting and device acceptance steps.

Target: Windows 11, x64 (minimum API baseline Windows 10 build 19041; older OS compatibility is not verified); BLE adapter for Bluetooth. Compatible Codex Buddy v1 firmware required. No administrator permissions, cloud account or separate .NET runtime installation is needed for the self-contained package.

Build with .NET 10 SDK:

```powershell
./scripts/build.ps1
```

This runs the dependency-free core checks and publishes a self-contained portable ZIP. ARM64 can be built with `-Runtime win-arm64`. Hardware and UI testing require a Windows machine; cross-compilation and host core tests do not establish USB/BLE hardware compatibility.

Existing macOS and firmware sources are not modified by this Windows implementation.
