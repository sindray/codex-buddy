# Codex Buddy · Windows

Windows 原生托盘助手，向 AI Passport 同步本机 Codex 的任务状态。当前版本 0.1.0。

## 功能

- 任务开始、思考、规划、读取、搜索、写代码、构建、测试、审查、等待回答、完成、失败和取消状态。
- 同步进行中的任务数、用时和项目名；不发送对话正文。
- USB、BLE 蓝牙；自动模式优先已收到设备回执的 USB，断线自动重试。
- 多个设备时手动选择，暂停／恢复同步、选择 Codex 记录目录。
- 本版本不包含字幕、键盘监听、主题同步、音量或熄屏配置；设备原有按键和固件功能继续使用。

## 使用

1. 面向 Windows 11，默认发行包为 x64；ARM64 需另行构建。代码使用的最低 Windows API 基线为 Windows 10 内部版本 19041，旧系统尚未实测，不承诺所有 Windows 10 版本兼容。
2. 解压完整文件夹，双击 `CodexBuddy.exe`。免安装、自带运行环境，在右下角托盘打开菜单；退出请使用托盘菜单。
3. 设备需已有兼容 Codex Buddy v1 协议的固件，当前项目 0.4.16 固件可对接，无需重新烧录。
4. USB：接入支持数据传输的线，程序识别 AI Passport 串口。关闭占用同一串口的串口工具或其他助手。
5. 蓝牙：电脑需支持 BLE 并打开蓝牙，设备保持通电；在“设置 → 连接方式”选择“仅蓝牙”即可扫描连接，无需先在系统页面配对。有多台设备时在“蓝牙设备”选择。设备一次只能连接一台电脑，另一台 Mac 助手需先退出或暂停。
6. 在 Codex 发起任务，托盘显示连接和任务状态，设备同步表情。首次连接以收到设备回执为准。

自动模式优先 USB。当设备正通过 USB 同步时，固件会暂时拒绝 BLE 状态覆盖；要验证纯蓝牙，请断开设备的数据 USB 连接，改用充电器供电。

## 目录和权限

自动读取绝对路径 `CODEX_HOME` 下的 `sessions`，否则读取用户目录 `.codex/sessions`；也可在设置中选择数据目录或 `sessions` 本身。只扫描 UTC 今天／昨天的会话文件；长时间未更新的任务会回到待机。

无需管理员、辅助功能或输入监控权限；Windows 若提示蓝牙设备访问，请允许。仅解析本地 Codex 任务记录，通过 USB 或 BLE 向设备发送状态、任务数、用时和项目文件夹名称，不上传到服务器，不监听正在输入的文字。

设置保存在 `%LOCALAPPDATA%\CodexBuddy\settings.json`。默认不开机启动。退出后删除解压目录即可卸载；要清除设置，再删除上述设置目录。

此开发包未使用商业代码签名证书，Windows 可能显示来源提示。请仅运行可信来源的包，核对发布方提供的 SHA256，无需关闭系统安全功能。

## 排查

- 无任务：先在同一 Windows 环境运行 Codex，再核对数据目录。WSL／网络共享目录未做实机验证，不保证自动发现。
- USB 无法打开：检查数据线、设备管理器中的 COM 口，以及是否被其他程序占用。只自动匹配 VID 303A / PID 1001；不修改其他串口设备。
- 蓝牙找不到：检查 BLE 适配器、系统蓝牙开关、设备是否已连接其他电脑。扫描需要几秒，打开菜单可刷新设备列表。
- 一直等待确认：设备须运行兼容固件；重启设备，托盘选择“设置 → 重新连接”。

## 构建与验证

需要 .NET 10 SDK。在 PowerShell 执行：

```powershell
./scripts/build.ps1
# 可选：./scripts/build.ps1 -Runtime win-arm64
```

脚本先运行核心逻辑检查，再生成包含运行环境的便携 ZIP。Windows UI、WMI 串口和 WinRT BLE 均须在 Windows 实机验收；Mac 上的逻辑测试和跨平台构建不等同于设备连通验证。

验收顺序：USB 收到回执 → 发起／结束任务 → 拔插重连 → 退出其他电脑助手 → 仅蓝牙重复任务 → 断电恢复 → 自动模式 USB 优先 → 多设备手动选择。持续运行至少 30 分钟观察状态及资源占用。

项目结构：`Core` 为协议和记录解析，`App` 为 Windows 托盘／USB／BLE，`Tests` 为核心检查；不修改现有 macOS 助手和设备固件。

系统支持范围还取决于 [.NET 对 Windows 的支持](https://learn.microsoft.com/en-us/dotnet/core/install/windows)。

技术参考：[Windows GATT 客户端](https://learn.microsoft.com/en-us/windows/apps/develop/devices-sensors/gatt-client)、[.NET 单文件部署](https://learn.microsoft.com/en-us/dotnet/core/deploying/single-file/overview)。
