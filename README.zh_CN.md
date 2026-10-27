**简体中文** · [English](README.md)

# Codex Buddy

**你写代码，它负责有反应。** 把 FoloToy AI Passport 变成桌边的代码搭子：思考时张望，测试过了得意，遇到问题还会“哔”你一下。

![Codex Buddy 功能展示](output/community/codex-buddy-poster-v4.png)

## 功能

- 任务表情、灵动眼神和不同情绪的 bit 音效。
- USB / BLE 同步，自动优先 USB，断线重连。
- 电量、任务状态、任务用时一眼看见。
- 音量可调（默认 10%）、静音、空闲降刷新、自动熄屏。
- 双击 OK 锁屏，任意功能键唤醒。
- Mac 专属：打招呼、摸摸头、小剧场；可选输入字幕和文字互动；外围状态区跟随系统深浅主题，脸屏始终保持深色；可配置熄屏时间。

| 系统 | 支持情况 |
| --- | --- |
| macOS 13+，Apple Silicon / Intel | 主要桌面助手；字幕可选，仅支持部分输入框。 |
| Windows x64 | **测试版**：已实现任务同步、USB 和 BLE，连接兼容性仍待 Windows 实机验证；暂无字幕、互动面板和主题同步。 |

## 开始使用

给 AI Passport 安装 Codex Buddy 固件，在运行 Codex 的同一台电脑打开助手，通过 USB 或蓝牙连接。助手需要持续运行。蓝牙直接在助手内连接，无需预先在系统里配对；设备一次只能连接一台蓝牙电脑。

本次首次公开源码，暂未提供可下载的 Release 安装包，请按下方指南自行构建。项目固件与 Windows 助手协议兼容；已有兼容的 Codex Buddy 固件时，换电脑不需要重新烧录。

## 构建与文档

| 部分 | 文档 | 构建 / 检查 |
| --- | --- | --- |
| 设备固件 | [Buddy 说明](firmware/docs/CODEX_BUDDY.zh_CN.md) · [环境准备](firmware/docs/development/engineering/environment-setup.zh_CN.md) | 激活 ESP-IDF 5.5.3 后，在 `firmware/` 运行 `./tools/validate.sh`。 |
| Mac 助手 | [使用与构建](desktop/README.zh_CN.md) | macOS 安装 Xcode 命令行工具后，运行 `bash desktop/scripts/test.sh`、`bash desktop/scripts/build.sh`。 |
| Windows 助手 | [使用与构建](desktop-windows/README.zh_CN.md) | Windows 安装 .NET 10 SDK，在 `desktop-windows/` 运行 `./scripts/build.ps1`。 |
| 社区物料 | [简短上架说明](output/community/community-post.txt) · [产品图](output/community/codex-buddy-poster-v4.png) | 产品图为效果示意，并非实机截图。 |

Mac 构建默认使用本机临时签名，不包含开发者私有签名配置；如需更新后维持权限，可按桌面文档配置稳定的本机签名。当前构建未经过 Apple 公证，也不是使用商业证书签名的 Windows 安装程序。

## 权限与隐私

- USB 任务同步无需辅助功能授权。
- 使用蓝牙时才需要蓝牙访问权限。
- Mac 输入字幕与文字互动**默认关闭**；开启后需辅助功能／设备控制和数据访问授权。仅支持部分输入框，不保证输入法组词逐字实时显示。
- 不需要麦克风、摄像头、屏幕录制或输入监控权限。
- 助手在本机解析 Codex 会话记录，将任务摘要直接发送至设备，不上传聊天内容到服务器。开启字幕后会向设备发送渲染后的字幕图像。

## 许可证与致谢

采用 [MIT 许可证](LICENSE)。固件基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)，保留原有[许可证](firmware/LICENSE)和声明。随附 Noto Sans SC 字体资源保留 [SIL Open Font License](firmware/assets/fonts/OFL.txt)。构建时下载的依赖遵守各自许可证。

本项目为独立社区作品，相关产品与公司名称归各自所有者。
