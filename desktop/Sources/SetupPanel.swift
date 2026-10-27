import AppKit

/// A connection checklist, kept separate from task and interaction controls.
final class SetupPanel: NSObject {
    let panel: NSPanel
    let source = NSTextField(wrappingLabelWithString: "正在检测 Codex…")
    let path = NSTextField(wrappingLabelWithString: "")
    let device = NSTextField(wrappingLabelWithString: "正在检测设备…")
    let login = NSButton(checkboxWithTitle: "登录时自动启动", target: nil, action: nil)
    let finish = NSButton(title: "完成并在后台运行", target: nil, action: nil)
    let devices = NSPopUpButton(frame: .zero, pullsDown: false)
    private var deviceListKey = ""
    var onChoose: (() -> Void)?
    var onDefault: (() -> Void)?
    var onSearch: (() -> Void)?
    var onPermission: (() -> Void)?
    var onLogin: (() -> Void)?
    var onFinish: (() -> Void)?
    var onDevice: ((String?) -> Void)?
    override init() {
        panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 480, height: 540), styleMask: [.titled, .closable], backing: .buffered, defer: false)
        super.init()
        panel.title = "连接你的 AI Passport"; panel.isReleasedWhenClosed = false
        panel.hidesOnDeactivate = false; panel.center()
        let content = NSView(); panel.contentView = content
        let stack = NSStackView(); stack.orientation = .vertical; stack.alignment = .leading; stack.spacing = 14
        stack.translatesAutoresizingMaskIntoConstraints = false; content.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 28), stack.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -28), stack.topAnchor.constraint(equalTo: content.topAnchor, constant: 25)])
        func label(_ text: String, size: CGFloat = 13, bold: Bool = false) -> NSTextField {
            let field = NSTextField(wrappingLabelWithString: text)
            field.font = .systemFont(ofSize: size, weight: bold ? .semibold : .regular)
            return field
        }
        func row(_ children: [NSView]) -> NSStackView { let r = NSStackView(views: children); r.spacing = 10; return r }
        func button(_ title: String, _ action: Selector) -> NSButton { NSButton(title: title, target: self, action: action) }
        stack.addArrangedSubview(label("让 Codex 的状态，出现在你身边", size: 19, bold: true))
        stack.addArrangedSubview(label("设备接通电源，电脑打开 Codex。完成下面两项检测后即可自动同步。"))
        stack.addArrangedSubview(label("1  本机 Codex", bold: true))
        stack.addArrangedSubview(source)
        path.font = .systemFont(ofSize: 11); path.textColor = .secondaryLabelColor; path.isSelectable = true
        stack.addArrangedSubview(path)
        stack.addArrangedSubview(row([button("选择 Codex 目录…", #selector(choose)), button("恢复自动检测", #selector(reset))]))
        stack.addArrangedSubview(label("可选择 .codex、自定义 CODEX_HOME 或其中的 sessions 文件夹。", size: 11))
        stack.addArrangedSubview(label("2  AI Passport", bold: true))
        stack.addArrangedSubview(device)
        devices.target = self; devices.action = #selector(selectDevice)
        stack.addArrangedSubview(devices)
        stack.addArrangedSubview(row([button("搜索蓝牙设备", #selector(search)), button("蓝牙权限设置…", #selector(permission))]))
        stack.addArrangedSubview(label("首次连接请允许系统蓝牙权限，无需在系统蓝牙列表配对。USB 插入后优先连接。", size: 11))
        login.target = self; login.action = #selector(toggleLogin); stack.addArrangedSubview(login)
        let privacy = label("仅在本机读取会话记录，向设备发送任务状态、数量、用时和项目名。不发送聊天内容，不需要 API Key。", size: 11)
        privacy.textColor = .secondaryLabelColor; stack.addArrangedSubview(privacy)
        finish.target = self; finish.action = #selector(done)
        stack.addArrangedSubview(row([finish, button("稍后设置", #selector(later))]))
        for view in stack.arrangedSubviews where view is NSTextField {
            view.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        }
    }
    func update(sourceHealth: SourceHealth, root: URL, connection: String, connected: Bool,
                bluetoothDevices: [(UUID, String)], selected: UUID?, loginEnabled: Bool, loginNeedsApproval: Bool) {
        source.stringValue = sourceHealth.message; path.stringValue = root.path
        source.textColor = sourceHealth == .ready ? .systemTeal : .secondaryLabelColor
        device.stringValue = connection; device.textColor = connected ? .systemTeal : .secondaryLabelColor
        finish.isEnabled = sourceHealth == .ready && connected
        login.state = loginEnabled ? .on : .off
        login.title = loginNeedsApproval ? "登录启动待批准 · 点击打开设置…" : "登录时自动启动"
        // Don't rebuild an open popup while the player is choosing a device.
        let key = bluetoothDevices.map { $0.0.uuidString + $0.1 }.joined(separator: "|") + (selected?.uuidString ?? "auto")
        if key != deviceListKey || devices.numberOfItems == 0 {
            deviceListKey = key
            devices.removeAllItems(); devices.addItem(withTitle: "自动选择单个蓝牙设备")
            var entries = bluetoothDevices
            if let selected, !entries.contains(where: { $0.0 == selected }) { entries.append((selected, "已记住的设备")) }
            for (id, name) in entries {
                devices.addItem(withTitle: name + " · " + String(id.uuidString.suffix(6)))
                devices.lastItem?.representedObject = id.uuidString
            }
            if let selected, let index = entries.firstIndex(where: { $0.0 == selected }) { devices.selectItem(at: index + 1) }
        }
    }
    func show() { NSApp.activate(ignoringOtherApps: true); panel.makeKeyAndOrderFront(nil) }
    @objc private func choose() { onChoose?() }
    @objc private func reset() { onDefault?() }
    @objc private func search() { onSearch?() }
    @objc private func permission() { onPermission?() }
    @objc private func toggleLogin() { onLogin?() }
    @objc private func selectDevice() { onDevice?(devices.selectedItem?.representedObject as? String) }
    @objc private func done() { onFinish?(); panel.close() }
    @objc private func later() { panel.close() }
}
