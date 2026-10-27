import AppKit

final class PetFace: NSView {
    var state = "idle"
    var onTouch: (() -> Void)?
    private var pointer = CGPoint.zero
    private var inside = false
    private var eyes = CGSize(width: 49, height: 67)
    private var gaze = CGPoint.zero
    private var touchedAt: TimeInterval = -100
    private var clock: Timer?
    override var isFlipped: Bool { true }
    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .mouseEnteredAndExited, .activeAlways, .inVisibleRect], owner: self))
    }
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow(); clock?.invalidate(); clock = nil
        guard window != nil else { return }
        let timer = Timer(timeInterval: 1.0 / 30, repeats: true) { [weak self] _ in
            guard let self, self.window?.isVisible == true else { return }
            self.animate()
        }
        clock = timer; RunLoop.main.add(timer, forMode: .common)
    }
    override func mouseEntered(with event: NSEvent) { inside = true; mouseMoved(with: event) }
    override func mouseExited(with event: NSEvent) { inside = false }
    override func mouseMoved(with event: NSEvent) { pointer = convert(event.locationInWindow, from: nil) }
    override func mouseDown(with event: NSEvent) { touchedAt = ProcessInfo.processInfo.systemUptime; onTouch?() }
    override func accessibilityPerformPress() -> Bool { touchedAt = ProcessInfo.processInfo.systemUptime; onTouch?(); return true }
    private func animate() {
        let now = ProcessInfo.processInfo.systemUptime
        let blinking = now.truncatingRemainder(dividingBy: 4.7) < 0.13
        let happy = state == "done" || now - touchedAt < 0.65
        let sleepy = state == "sleeping"
        let target = CGSize(width: happy ? 57 : 49, height: blinking ? 5 : sleepy ? 10 : happy ? 23 : 67)
        eyes.width += (target.width - eyes.width) * 0.28
        eyes.height += (target.height - eyes.height) * 0.32
        let targetX = inside ? max(-9, min(9, (pointer.x - bounds.midX) / 13)) : sin(now * 0.55) * 3
        let targetY = inside ? max(-6, min(6, (pointer.y - bounds.midY) / 13)) : sin(now * 0.8) * 1.5
        gaze.x += (targetX - gaze.x) * 0.17; gaze.y += (targetY - gaze.y) * 0.17
        needsDisplay = true
    }
    override func draw(_ dirtyRect: NSRect) {
        NSColor(calibratedRed: 0.025, green: 0.04, blue: 0.05, alpha: 1).setFill()
        NSBezierPath(roundedRect: bounds.insetBy(dx: 1, dy: 1), xRadius: 26, yRadius: 26).fill()
        NSColor(calibratedRed: 0.43, green: 0.85, blue: 0.89, alpha: 1).setFill()
        let now = ProcessInfo.processInfo.systemUptime
        for (i, offset) in [-47.0, 47.0].enumerated() {
            // Curious eyes move vertically; no masks or clipped half-eyes.
            let curious = state == "thinking" || state == "searching"
            let y = bounds.midY + gaze.y + (curious ? (i == 0 ? -5.0 : 5.0) : 0)
            let wave = state == "waving" ? sin(now * 5) * 3 : 0
            let rect = NSRect(x: bounds.midX + offset + gaze.x - eyes.width / 2,
                              y: y + wave - eyes.height / 2, width: eyes.width, height: eyes.height)
            let radius = min(eyes.width / 2, eyes.height / 2)
            NSBezierPath(roundedRect: rect, xRadius: radius, yRadius: radius).fill()
        }
    }
    deinit { clock?.invalidate() }
}

final class PetPanel: NSObject {
    let panel: NSPanel
    let face = PetFace(frame: .zero)
    let message = NSTextField(labelWithString: "点点眼睛，摸摸头")
    let task = NSTextField(labelWithString: "")
    let connection = NSTextField(labelWithString: "")
    let stopButton = NSButton(title: "回到任务", target: nil, action: nil)
    var onAction: ((PetAction) -> Void)?
    var onTouch: (() -> Void)?
    var onStop: (() -> Void)?
    private var buttons: [NSButton] = []
    override init() {
        panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 374, height: 398), styleMask: [.titled, .closable], backing: .buffered, defer: false)
        super.init()
        panel.title = "陪 Buddy 玩一会儿"; panel.isReleasedWhenClosed = false
        panel.hidesOnDeactivate = false; panel.center()
        let content = NSView(); panel.contentView = content
        let stack = NSStackView(); stack.orientation = .vertical; stack.alignment = .centerX; stack.spacing = 13
        stack.translatesAutoresizingMaskIntoConstraints = false; content.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 24), stack.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -24), stack.topAnchor.constraint(equalTo: content.topAnchor, constant: 20)])
        face.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([face.widthAnchor.constraint(equalToConstant: 326), face.heightAnchor.constraint(equalToConstant: 152)])
        face.setAccessibilityElement(true); face.setAccessibilityLabel("Buddy 的眼睛，点击摸摸头"); face.setAccessibilityRole(.button)
        face.onTouch = { [weak self] in self?.onTouch?() }
        stack.addArrangedSubview(face)
        message.font = .systemFont(ofSize: 14, weight: .medium); stack.addArrangedSubview(message)
        task.font = .systemFont(ofSize: 11); task.textColor = .secondaryLabelColor; stack.addArrangedSubview(task)
        let row = NSStackView(); row.orientation = .horizontal; row.spacing = 7
        for action in [PetAction.wave, .praise, .curious, .nap] {
            let b = NSButton(title: action.title, target: self, action: #selector(play(_:))); b.identifier = NSUserInterfaceItemIdentifier(action.rawValue); b.bezelStyle = .rounded; buttons.append(b); row.addArrangedSubview(b)
        }
        stack.addArrangedSubview(row)
        let bottom = NSStackView(); bottom.orientation = .horizontal; bottom.spacing = 12
        let theatre = NSButton(title: "随机小剧场", target: self, action: #selector(play(_:))); theatre.identifier = NSUserInterfaceItemIdentifier(PetAction.theatre.rawValue); theatre.bezelStyle = .rounded; buttons.append(theatre)
        stopButton.target = self; stopButton.action = #selector(stop); stopButton.bezelStyle = .rounded
        bottom.addArrangedSubview(theatre); bottom.addArrangedSubview(stopButton); stack.addArrangedSubview(bottom)
        connection.font = .systemFont(ofSize: 10); connection.textColor = .secondaryLabelColor; stack.addArrangedSubview(connection)
    }
    @objc private func play(_ sender: NSButton) { if let value = sender.identifier?.rawValue, let action = PetAction(rawValue: value) { onAction?(action) } }
    @objc private func stop() { onStop?() }
    func show() { NSApp.activate(ignoringOtherApps: true); panel.makeKeyAndOrderFront(nil) }
    func update(base: BuddyPacket, moment: PetMoment?, status: String, connected: Bool, running: Bool) {
        face.state = moment?.state ?? base.state
        let urgent = PetInteraction.urgent(base)
        message.stringValue = moment?.title ?? (urgent ? "先处理任务，我在这里等你" : "点点眼睛，摸摸头")
        task.stringValue = "真实任务 · \(stateNames[base.state] ?? base.state) · \(base.active) 个进行中"
        connection.stringValue = connected ? status : "\(status) · 互动仅在电脑预览"
        buttons.forEach { $0.isEnabled = running && !urgent }
        stopButton.isEnabled = moment != nil
    }
}
