import AppKit
import ServiceManagement
import Darwin

let stateNames = ["boot":"启动", "connecting":"等待连接", "idle":"待命", "received":"收到任务", "thinking":"思考中", "planning":"规划中", "reading":"阅读中", "searching":"搜索中", "coding":"编写代码", "building":"构建中", "testing":"测试中", "reviewing":"检查中", "waiting":"等待回复", "approval":"等待确认", "done":"已完成", "error":"遇到问题", "cancelled":"已取消", "reconnecting":"重新连接", "sleeping":"休息中", "waving":"打招呼"]

final class BuddyHeader: NSView {
    var subtitle = "你的 Codex，陪在桌面上" { didSet { needsDisplay = true } }
    var connected = false { didSet { needsDisplay = true } }
    var onOpen: (() -> Void)?
    override func mouseDown(with event: NSEvent) { onOpen?() }
    override var isFlipped: Bool { true }
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        let face = NSRect(x: 17, y: 17, width: 49, height: 43)
        NSColor(calibratedWhite: 0.065, alpha: 1).setFill()
        NSBezierPath(roundedRect: face, xRadius: 14, yRadius: 14).fill()
        (connected ? NSColor.systemTeal : NSColor.secondaryLabelColor).setFill()
        for x in [26.0, 44.0] { NSBezierPath(roundedRect: NSRect(x: x, y: 29, width: 12, height: 17), xRadius: 6, yRadius: 6).fill() }
        let title = "Codex Buddy" as NSString
        title.draw(at: NSPoint(x: 78, y: 20), withAttributes: [.font: NSFont.systemFont(ofSize: 16, weight: .semibold), .foregroundColor: NSColor.labelColor])
        (subtitle as NSString).draw(in: NSRect(x: 78, y: 44, width: 210, height: 35), withAttributes: [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.secondaryLabelColor])
    }
}

final class BuddyApp: NSObject, NSApplicationDelegate, NSMenuDelegate {
    let defaults = UserDefaults.standard
    let usb = USBLink(), ble = BLELink()
    let monitor = StateMonitor(root: CodexLocation.resolve(saved: UserDefaults.standard.string(forKey: "sessionsPath")))
    let setup = SetupPanel()
    var sourceHealth: SourceHealth = .checking
    var sourceRoot = CodexLocation.resolve(saved: UserDefaults.standard.string(forKey: "sessionsPath"))
    var sourceGeneration = 0
    let work = DispatchQueue(label: "com.johnny.codexbuddy.monitor", qos: .utility)
    var polling = false
    var packet = BuddyPacket()
    var interaction = PetInteraction()
    let textInput = TextInputMonitor()
    var textInteractionEnabled: Bool { defaults.bool(forKey: "textInteractionEnabled") }
    var textItem: NSMenuItem!
    var moment: PetMoment?
    let petPanel = PetPanel()
    var connected: Bool {
        let now = Date().timeIntervalSince1970
        return running && ((usb.path != nil && usbReceipt.confirmed(now: now)) || (ble.ready && bleReceipt.confirmed(now: now)))
    }
    var usbReceipt = ReceiptTracker(), bleReceipt = ReceiptTracker()
    var lastUSBState = ""
    var seq = Int.random(in: 10000...1000000)
    var lastUSBSend: TimeInterval = 0, lastBLESend: TimeInterval = 0
    var usbAttempt: TimeInterval = 0, bleAttempt: TimeInterval = 0
    var usbRetry: TimeInterval = 0
    var previousState = ""
    var lastUSBCaption = "", lastBLECaption = ""
    var lastUSBTheme = "", lastBLETheme = ""
    var systemTheme: String { NSApp.effectiveAppearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua ? "dark" : "light" }
    var captionText = "", captionBitmap = ""
    var lastDiscovery: TimeInterval = 0, lastPoll: TimeInterval = 0, lastDiagnostic: TimeInterval = 0
    var devices: [USBDevice] = []
    var timer: Timer?
    var item: NSStatusItem!
    let menu = NSMenu()
    let header = BuddyHeader(frame: NSRect(x: 0, y: 0, width: 310, height: 85))
    let connectionRow = NSMenuItem(title: "正在准备连接…", action: nil, keyEquivalent: "")
    let taskRow = NSMenuItem(title: "任务 · 待命", action: nil, keyEquivalent: "")
    let sourceRow = NSMenuItem(title: "正在读取本机 Codex 状态…", action: nil, keyEquivalent: "")
    let settingsMenu = NSMenu(), helpMenu = NSMenu()
    let modeMenu = NSMenu(), usbMenu = NSMenu(), bleMenu = NSMenu(), screenMenu = NSMenu()
    let screenOptions = [("1 分钟",60),("3 分钟",180),("5 分钟（默认）",300),("10 分钟",600),("30 分钟",1800),("永不熄屏",0)]
    var screenOffSeconds: Int {
        let value = defaults.object(forKey: "screenOffSeconds") as? Int ?? 300
        return screenOptions.contains(where: { $0.1 == value }) ? value : 300
    }
    var pauseItem: NSMenuItem!, loginItem: NSMenuItem!
    var running = true
    var lockFD: Int32 = -1
    var mode: String { get { defaults.string(forKey: "transport") ?? "auto" } set { defaults.set(newValue, forKey: "transport") } }
    var selectedUSB: String? { get { defaults.string(forKey: "usbPath") } set { defaults.set(newValue, forKey: "usbPath") } }
    var statusURL: URL {
        FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/CodexBuddy/status.json")
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        lockFD = open("/tmp/codex-buddy-desktop.lock", O_CREAT | O_RDWR | O_NOFOLLOW, 0o600)
        guard lockFD >= 0, flock(lockFD, LOCK_EX | LOCK_NB) == 0 else { NSApp.terminate(nil); return }
        NSApp.setActivationPolicy(.accessory)
        if let value = defaults.string(forKey: "bleID") { ble.selected = UUID(uuidString: value) }
        item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        item.button?.image = NSImage(size: NSSize(width: 22, height: 18), flipped: false) { _ in
            NSColor.controlTextColor.setFill()
            for x in [3.0, 13.0] { NSBezierPath(roundedRect: NSRect(x: x, y: 4, width: 6, height: 10), xRadius: 3, yRadius: 3).fill() }
            return true
        }
        item.button?.image?.isTemplate = true
        item.button?.setAccessibilityLabel("Codex Buddy")
        let top = NSMenuItem(); top.view = header; menu.addItem(top)
        menu.addItem(action("陪 Buddy 玩…", #selector(showPet)))
        pauseItem = action("暂停同步", #selector(togglePause)); menu.addItem(pauseItem)
        menu.addItem(.separator())
        [connectionRow, taskRow, sourceRow].forEach { $0.isEnabled = false; settingsMenu.addItem($0) }
        settingsMenu.addItem(.separator())
        for (title, value) in [("自动 · USB 优先", "auto"), ("仅 USB", "usb"), ("仅蓝牙", "ble")] {
            let choice = action(title, #selector(changeMode(_:))); choice.representedObject = value; modeMenu.addItem(choice)
        }
        submenu("连接方式", modeMenu, in: settingsMenu)
        submenu("USB 设备", usbMenu, in: settingsMenu)
        submenu("蓝牙设备", bleMenu, in: settingsMenu)
        settingsMenu.addItem(action("重新连接", #selector(reconnect)))
        settingsMenu.addItem(.separator())
        settingsMenu.addItem(action("连接引导…", #selector(showSetup)))
        settingsMenu.addItem(action("选择 Codex 目录…", #selector(chooseSource)))
        textItem = action("文字互动与字幕", #selector(toggleTextInteraction)); settingsMenu.addItem(textItem)
        loginItem = action("登录时自动启动", #selector(toggleLogin)); settingsMenu.addItem(loginItem)
        for (title, seconds) in screenOptions {
            let choice = action(title, #selector(changeScreenTimeout(_:)))
            choice.representedObject = seconds; screenMenu.addItem(choice)
        }
        submenu("空闲自动熄屏", screenMenu, in: settingsMenu)
        submenu("设置", settingsMenu)
        helpMenu.addItem(action("使用帮助…", #selector(help)))
        helpMenu.addItem(action("蓝牙权限设置…", #selector(bluetoothSettings)))
        helpMenu.addItem(action("关于 Codex Buddy", #selector(about)))
        submenu("帮助", helpMenu)
        menu.addItem(.separator())
        let quit = action("退出 Codex Buddy", #selector(quit)); quit.keyEquivalent = "q"; menu.addItem(quit)
        menu.delegate = self; item.menu = menu
        header.onOpen = { [weak self] in self?.menu.cancelTracking(); self?.showPet() }
        setup.onChoose = { [weak self] in self?.chooseSource() }
        setup.onDefault = { [weak self] in self?.changeSource(nil) }
        setup.onSearch = { [weak self] in self?.searchBLE() }
        setup.onPermission = { [weak self] in self?.bluetoothSettings() }
        setup.onLogin = { [weak self] in self?.toggleLogin(); self?.refreshSetup() }
        setup.onDevice = { [weak self] value in
            guard let self else { return }
            self.defaults.set(value, forKey: "bleID"); self.ble.selected = value.flatMap(UUID.init(uuidString:)); self.reconnect()
        }
        setup.onFinish = { [weak self] in self?.defaults.set(true, forKey: "setupComplete") }
        petPanel.onAction = { [weak self] in self?.play($0) }
        petPanel.onTouch = { [weak self] in
            guard let self, self.running else { return }
            _ = self.interaction.touch(base: self.packet, connected: self.connected, now: Date().timeIntervalSince1970)
            self.lastUSBSend = 0; self.lastBLESend = 0; self.tick()
        }
        petPanel.onStop = { [weak self] in self?.interaction.stop(); self?.tick() }
        usb.onReply = { [weak self] value in guard let self else { return }; if self.usbReceipt.receive(value, now: Date().timeIntervalSince1970), value["buddy_ack"] != nil { self.lastUSBState = value["state"] as? String ?? "" } }
        ble.onReply = { [weak self] value in guard let self else { return }; _ = self.bleReceipt.receive(value, now: Date().timeIntervalSince1970) }
        let t = Timer(timeInterval: 0.5, repeats: true) { [weak self] _ in self?.tick() }
        timer = t; RunLoop.main.add(t, forMode: .common)
        NSWorkspace.shared.notificationCenter.addObserver(self, selector: #selector(wake), name: NSWorkspace.didWakeNotification, object: nil)
        tick()
        if !defaults.bool(forKey: "setupComplete") || CommandLine.arguments.contains("--setup") { showSetup() }
        if CommandLine.arguments.contains("--interact") { showPet() }
    }
    func action(_ title: String, _ selector: Selector) -> NSMenuItem {
        let m = NSMenuItem(title: title, action: selector, keyEquivalent: ""); m.target = self; return m
    }
    func submenu(_ title: String, _ child: NSMenu, in parent: NSMenu? = nil) { let m = NSMenuItem(title: title, action: nil, keyEquivalent: ""); m.submenu = child; (parent ?? menu).addItem(m) }
    func tick() {
        let now = Date().timeIntervalSince1970
        if now - lastPoll >= 1 && !polling {
            polling = true; lastPoll = now
            let generation = sourceGeneration
            work.async { [weak self] in
                guard let self else { return }
                let next = self.monitor.poll()
                let health = self.monitor.health
                DispatchQueue.main.async { [weak self] in
                    guard let self else { return }
                    self.polling = false
                    guard generation == self.sourceGeneration else { return }
                    self.packet = next; self.sourceHealth = health
                    self.sourceRow.title = health.message
                    self.refreshSetup()
                }
            }
        }
        if now - lastDiscovery >= 3 { devices = USBLink.devices(); lastDiscovery = now }
        if !running { interaction.stop() }
        moment = interaction.current(base: packet, connected: connected, now: now)
        if let action = textInput.poll(enabled: textInteractionEnabled && running && connected,
                                       allowed: moment == nil && !PetInteraction.urgent(packet), now: now) {
            _ = interaction.start(action, base: packet, connected: connected, now: now)
            moment = interaction.current(base: packet, connected: connected, now: now)
        }
        var outgoing = interaction.packet(base: packet, moment: moment)
        if captionText != textInput.subtitleText {
            captionText = textInput.subtitleText; captionBitmap = CaptionRenderer.encode(captionText)
        }
        outgoing.caption = captionBitmap
        outgoing.theme = systemTheme
        outgoing.screenOffSeconds = screenOffSeconds
        if running {
            if mode != "ble" {
                if let path = usb.path, !devices.contains(where: { $0.path == path }) { usb.disconnect(); usbReceipt = ReceiptTracker() }
                if usb.path == nil && now >= usbRetry {
                    let candidates = devices.filter { selectedUSB == nil || $0.path == selectedUSB }
                    if candidates.count == 1, usb.connect(candidates[0]) {
                        usbReceipt = ReceiptTracker(); usbAttempt = now; lastUSBSend = 0
                    } else { usbRetry = now + 3 }
                }
                usb.flush()
                if usb.path != nil {
                    if now - lastUSBSend >= 2 || previousState != outgoing.state || lastUSBCaption != outgoing.caption || lastUSBTheme != outgoing.theme {
                        seq = (seq + 1) % 2147483647; usbReceipt.sent(seq); usb.send(outgoing.encoded(seq: seq)); lastUSBSend = now; lastUSBCaption = outgoing.caption; lastUSBTheme = outgoing.theme
                    }
                    if now - max(usbAttempt, usbReceipt.lastAccepted) > 10 {
                        usb.disconnect(); usbReceipt = ReceiptTracker(); usbRetry = now + 5
                    }
                }
            }
            let usbGood = usb.path != nil && usbReceipt.confirmed(now: now)
            if mode != "usb" && !usbGood {
                if !ble.enabled { ble.start(); bleReceipt = ReceiptTracker(); bleAttempt = now }
                ble.tick(now: now)
                if ble.ready {
                    if now - lastBLESend >= 2 || previousState != outgoing.state || lastBLECaption != outgoing.caption || lastBLETheme != outgoing.theme {
                        seq = (seq + 1) % 2147483647
                        if ble.send(outgoing.encoded(seq: seq)) {
                            bleReceipt.sent(seq); lastBLESend = now; lastBLECaption = outgoing.caption; lastBLETheme = outgoing.theme
                        }
                    }
                    if now - max(bleAttempt, bleReceipt.lastResponse) > 25 {
                        ble.retry(); bleReceipt = ReceiptTracker(); bleAttempt = now
                    }
                } else { bleReceipt = ReceiptTracker(); bleAttempt = now; lastBLESend = 0 }
            } else if ble.enabled { ble.stop(); bleReceipt = ReceiptTracker() }
        }
        previousState = outgoing.state
        updateStatus(now)
        if now - lastDiagnostic >= 2 { diagnostic(now); lastDiagnostic = now }
    }
    var connectionText: String {
        let now = Date().timeIntervalSince1970
        if !running { return "同步已暂停" }
        if usb.path != nil && usbReceipt.confirmed(now: now) { return "USB 已连接 · 正在同步" }
        if ble.ready && bleReceipt.confirmed(now: now) { return "蓝牙已连接 · 正在同步" }
        if mode != "usb" && bleReceipt.busy { return "设备正由另一条 USB 连接同步" }
        if mode == "usb" { return devices.count > 1 && selectedUSB == nil ? "发现多个 USB 设备，请选择" : usb.status }
        if ble.enabled || mode == "ble" { return ble.status }
        if devices.count > 1 && selectedUSB == nil { return "发现多个 USB 设备，请选择" }
        return usb.status
    }
    func updateStatus(_ now: TimeInterval) {
        let connected = running && ((usb.path != nil && usbReceipt.confirmed(now: now)) || (ble.ready && bleReceipt.confirmed(now: now)))
        header.connected = connected
        let transport = usb.path != nil && usbReceipt.confirmed(now: now) ? "USB" : "蓝牙"
        let summary = connected ? "\(transport) 已连接 · \(stateNames[packet.state] ?? packet.state)" : connectionText
        let detail = moment.map { "互动 · " + $0.title } ?? (connected ? (packet.active > 0 ? "\(packet.active) 个任务进行中" : "等待新任务") : sourceHealth.message)
        header.subtitle = summary + "\n" + detail
        petPanel.update(base: packet, moment: moment, status: connectionText, connected: connected, running: running)
        connectionRow.title = connectionText
        taskRow.title = "任务 · \(stateNames[packet.state] ?? packet.state)  ·  \(packet.active) 个进行中"
        item.button?.toolTip = "Codex Buddy\n\(connectionText)\n\(stateNames[packet.state] ?? packet.state)"
        pauseItem.title = running ? "暂停同步" : "继续同步"
        refreshSetup()
    }
    func menuWillOpen(_ menu: NSMenu) {
        for m in screenMenu.items { m.state = (m.representedObject as? Int) == screenOffSeconds ? .on : .off }
        textItem.state = textInteractionEnabled ? .on : .off
        textItem.title = textInteractionEnabled && !TextInputMonitor.trusted ? "文字互动与字幕 · 等待辅助功能授权" : "文字互动与字幕"
        for m in modeMenu.items { m.state = (m.representedObject as? String) == mode ? .on : .off }
        usbMenu.removeAllItems()
        let autoUSB = action("自动选择单个设备", #selector(selectUSB(_:))); autoUSB.state = selectedUSB == nil ? .on : .off; usbMenu.addItem(autoUSB)
        for d in devices {
            let m = action(d.name, #selector(selectUSB(_:))); m.representedObject = d.path; m.state = selectedUSB == d.path ? .on : .off; usbMenu.addItem(m)
        }
        if devices.isEmpty { let m = NSMenuItem(title: "未发现 USB 设备", action: nil, keyEquivalent: ""); m.isEnabled = false; usbMenu.addItem(m) }
        bleMenu.removeAllItems()
        let autoBLE = action("自动选择单个设备", #selector(selectBLE(_:))); autoBLE.state = ble.selected == nil ? .on : .off; bleMenu.addItem(autoBLE)
        var bluetoothDevices = ble.devices
        if let selected = ble.selected, !bluetoothDevices.contains(where: { $0.0 == selected }) { bluetoothDevices.append((selected, "已记住的 CodexBuddy")) }
        for (id, name) in bluetoothDevices {
            let m = action(name + " · " + String(id.uuidString.suffix(6)), #selector(selectBLE(_:))); m.representedObject = id.uuidString; m.state = ble.selected == id ? .on : .off; bleMenu.addItem(m)
        }
        bleMenu.addItem(action("搜索蓝牙设备…", #selector(searchBLE)))
        loginItem.state = SMAppService.mainApp.status == .enabled ? .on : .off
        loginItem.title = SMAppService.mainApp.status == .requiresApproval ? "登录启动待批准 · 打开设置…" : "登录时自动启动"
    }
    func refreshSetup() {
        setup.update(sourceHealth: sourceHealth, root: sourceRoot, connection: connectionText, connected: connected,
                     bluetoothDevices: ble.devices, selected: ble.selected,
                     loginEnabled: SMAppService.mainApp.status == .enabled,
                     loginNeedsApproval: SMAppService.mainApp.status == .requiresApproval)
    }
    @objc func showSetup() { refreshSetup(); setup.show() }
    @objc func chooseSource() {
        let picker = NSOpenPanel(); picker.title = "选择 Codex 数据目录"
        picker.message = "选择 .codex、自定义 CODEX_HOME，或其中的 sessions 文件夹。"
        picker.canChooseDirectories = true; picker.canChooseFiles = false
        picker.allowsMultipleSelection = false; picker.showsHiddenFiles = true
        picker.directoryURL = sourceRoot.deletingLastPathComponent()
        picker.begin { [weak self] result in
            guard result == .OK, let folder = picker.url else { return }
            let root = CodexLocation.sessions(in: folder)
            var isDirectory: ObjCBool = false
            guard FileManager.default.fileExists(atPath: root.path, isDirectory: &isDirectory), isDirectory.boolValue,
                  (try? FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil)) != nil else {
                self?.alert("这个目录无法读取 Codex 记录", "请选择包含 sessions 的 Codex 数据目录，或直接选择 sessions 文件夹。原来的设置没有改变。")
                return
            }
            self?.changeSource(root)
        }
    }
    func changeSource(_ root: URL?) {
        defaults.set(root?.path, forKey: "sessionsPath")
        sourceRoot = root ?? CodexLocation.resolve(saved: nil)
        let selectedRoot = sourceRoot
        sourceGeneration += 1; packet = BuddyPacket(); interaction.stop()
        sourceHealth = .checking; sourceRow.title = "正在检测 Codex 目录…"
        work.async { [weak self] in self?.monitor.setRoot(selectedRoot) }
        lastPoll = 0; lastUSBSend = 0; lastBLESend = 0
        tick()
    }
    @objc func showPet() { petPanel.update(base: packet, moment: moment, status: connectionText, connected: connected, running: running); petPanel.show() }
    @objc func playInteraction(_ sender: NSMenuItem) {
        if let value = sender.representedObject as? String, let kind = PetAction(rawValue: value) { play(kind); showPet() }
    }
    func play(_ kind: PetAction) {
        guard running else { return }
        _ = interaction.start(kind, base: packet, connected: connected, now: Date().timeIntervalSince1970)
        lastUSBSend = 0; lastBLESend = 0; tick()
    }
    @objc func changeScreenTimeout(_ sender: NSMenuItem) {
        guard let seconds = sender.representedObject as? Int, screenOptions.contains(where: { $0.1 == seconds }) else { return }
        defaults.set(seconds, forKey: "screenOffSeconds")
        lastUSBSend = 0; lastBLESend = 0; tick()
    }
    @objc func changeMode(_ sender: NSMenuItem) { mode = sender.representedObject as? String ?? "auto"; reconnect() }
    @objc func selectUSB(_ sender: NSMenuItem) { selectedUSB = sender.representedObject as? String; reconnect() }
    @objc func selectBLE(_ sender: NSMenuItem) { defaults.set(sender.representedObject as? String, forKey: "bleID"); ble.selected = (sender.representedObject as? String).flatMap(UUID.init(uuidString:)); reconnect() }
    @objc func reconnect() { interaction.stop(); usb.disconnect(); ble.stop(); usbReceipt = ReceiptTracker(); bleReceipt = ReceiptTracker(); usbRetry = 0; lastUSBSend = 0; lastBLESend = 0; lastDiscovery = 0; tick() }
    @objc func searchBLE() { mode = "ble"; reconnect() }
    @objc func wake() { reconnect() }
    @objc func togglePause() { running.toggle(); if !running { usb.disconnect(); ble.stop() }; tick() }
    @objc func toggleTextInteraction() {
        if textInteractionEnabled {
            defaults.set(false, forKey: "textInteractionEnabled"); textInput.reset(); tick(); return
        }
        NSApp.activate(ignoringOtherApps: true)
        let prompt = NSAlert()
        prompt.messageText = "让 Buddy 回应你正在写的文字？"
        prompt.informativeText = "开启后，会读取当前输入框光标前最多 64 个字符，在本机识别“哈哈、好困、加油、？？”等短语。文字不保存、不上传到云端，屏幕底部会实时显示最近输入的内容，停笔约 5 秒后消失；设备通过 USB/蓝牙接收字幕图像和表情。密码框和系统安全输入期间跳过。\n\n需要在 macOS“隐私与安全性 → 辅助功能”中允许 Codex Buddy。部分应用不支持读取。可随时在这里关闭。"
        prompt.addButton(withTitle: "开启文字互动与字幕"); prompt.addButton(withTitle: "暂不开启")
        guard prompt.runModal() == .alertFirstButtonReturn else { return }
        defaults.set(true, forKey: "textInteractionEnabled"); textInput.reset()
        TextInputMonitor.requestPermission()
    }
    @objc func toggleLogin() {
        do {
            switch SMAppService.mainApp.status {
            case .enabled: try SMAppService.mainApp.unregister()
            case .requiresApproval: SMAppService.openSystemSettingsLoginItems()
            default: try SMAppService.mainApp.register()
            }
        } catch { alert("无法更新登录启动", error.localizedDescription + "\n建议先把应用放进“应用程序”文件夹，再启用此选项。") }
    }
    @objc func bluetoothSettings() { if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Bluetooth") { NSWorkspace.shared.open(url) } }
    @objc func help() {
        alert("使用 Codex Buddy", "设备接通电源，打开本机 Codex 即可。\n\n自动模式：USB 优先，拔掉数据线后改用蓝牙。首次蓝牙连接请允许系统权限；设备一次只连接一台电脑。\n\n发现多个设备时，请在“设置”的 USB/蓝牙设备列表中选择。另一台电脑或旧桥接脚本占用连接时，请先退出它。\n\n默认只发送任务状态、数量、用时和项目名。主动开启“文字互动与字幕”后，还会读取当前输入框末尾并向设备发送字幕图像；可随时关闭。开机启动可在“设置”中开启。")
    }
    @objc func about() { alert("Codex Buddy · 0.4.7", "AI Passport 的 Mac 连接工具\nUSB + Bluetooth LE\n\nmacOS 13 或更新版本，无需 Python。\n本机任务同步 · 可选文字互动与字幕。") }
    func alert(_ title: String, _ detail: String) { NSApp.activate(ignoringOtherApps: true); let a = NSAlert(); a.messageText = title; a.informativeText = detail; a.addButton(withTitle: "好"); a.runModal() }
    @objc func quit() { NSApp.terminate(nil) }
    func applicationWillTerminate(_ notification: Notification) { timer?.invalidate(); usb.disconnect(); ble.stop(); if lockFD >= 0 { Darwin.close(lockFD) } }
    func diagnostic(_ now: TimeInterval) {
        let value: [String: Any] = ["version": "0.4.7", "timestamp": now, "mode": mode, "running": running,
            "status": connectionText, "state": packet.state, "active": packet.active, "displayState": moment?.state ?? packet.state, "interaction": moment?.title ?? "",
            "usbConfirmed": usb.path != nil && usbReceipt.confirmed(now: now),
            "bleConfirmed": ble.ready && bleReceipt.confirmed(now: now), "bleStatus": ble.status,
            "theme": systemTheme, "screenOffSeconds": screenOffSeconds, "textEnabled": textInteractionEnabled, "textPermission": TextInputMonitor.trusted,
            "textInputStatus": textInput.status, "captionReady": !captionBitmap.isEmpty,
            "sourceHealth": sourceHealth.rawValue, "setupComplete": defaults.bool(forKey: "setupComplete"),
            "usbDevices": devices.map { $0.path }, "lastUSBState": lastUSBState]
        if let data = try? JSONSerialization.data(withJSONObject: value, options: [.prettyPrinted, .sortedKeys]) {
            try? FileManager.default.createDirectory(at: statusURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            try? data.write(to: statusURL, options: .atomic)
        }
    }
}

let app = NSApplication.shared
let delegate = BuddyApp()
app.delegate = delegate
app.run()
