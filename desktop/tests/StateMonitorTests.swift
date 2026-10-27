import Foundation

@main struct Tests {
    static var checks = 0
    static func check(_ value: @autoclosure () -> Bool, _ label: String) {
        checks += 1
        if !value() { fputs("FAIL: \(label)\n", stderr); exit(1) }
    }
    static func record(_ kind: String, _ payload: [String: Any], _ stamp: TimeInterval) -> [String: Any] {
        let f = ISO8601DateFormatter()
        return ["type": kind, "payload": payload, "timestamp": f.string(from: Date(timeIntervalSince1970: stamp))]
    }
    static func main() throws {
        var themedPacket = BuddyPacket()
        let darkWire = try JSONSerialization.jsonObject(with: themedPacket.encoded(seq: 1)) as! [String: Any]
        check(darkWire["theme"] as? String == "dark", "Default theme is compatible dark")
        themedPacket.theme = "light"
        let lightWire = try JSONSerialization.jsonObject(with: themedPacket.encoded(seq: 2)) as! [String: Any]
        check(lightWire["theme"] as? String == "light" && lightWire["state"] as? String == "idle", "Theme travels with task packet")
        check(darkWire["screenOffSeconds"] as? Int == 300, "Default idle screen timeout")
        themedPacket.screenOffSeconds = 0
        let noOffWire = try JSONSerialization.jsonObject(with: themedPacket.encoded(seq: 3)) as! [String: Any]
        check(noOffWire["screenOffSeconds"] as? Int == 0, "Never off travels in packet")
        let now: TimeInterval = 1800000000
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("codex-buddy-tests-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: root) }
        let session = RolloutSession(root.appendingPathComponent("test.jsonl"))
        session.consume(record("session_meta", ["cwd": "/code/project"], now), now: now)
        session.consume(record("event_msg", ["type": "task_started", "turn_id": "new"], now), now: now)
        session.consume(record("event_msg", ["type": "task_complete", "turn_id": "old"], now+1), now: now+1)
        check(session.active, "Late completion cannot end newer turn")
        session.consume(record("response_item", ["type": "function_call", "call_id": "a", "name": "exec_command", "arguments": "pytest tests"], now+2), now: now+2)
        check(session.state == "testing", "Tool classification")
        session.consume(record("response_item", ["type": "function_call", "call_id": "b", "name": "request_user_input"], now+3), now: now+3)
        session.consume(record("response_item", ["type": "function_call_output", "call_id": "a"], now+4), now: now+4)
        check(session.state == "waiting", "Parallel outstanding tool retains waiting")
        session.consume(record("response_item", ["type": "function_call_output", "call_id": "b"], now+5), now: now+5)
        check(session.state == "thinking", "Return to thinking after all tools finish")
        let monitor = StateMonitor(root: root, launched: now-1)
        monitor.sessions[session.url] = session
        var packet = monitor.poll(now: now+6)
        check(packet.state == "thinking" && packet.active == 1 && packet.task == "project" && packet.elapsed == 6, "Active metadata")
        session.consume(record("event_msg", ["type": "task_complete", "turn_id": "new", "error": NSNull()], now+7), now: now+7)
        check(session.state == "done", "Null error does not mean failure")
        packet = monitor.poll(now: now+8)
        check(packet.state == "done" && packet.active == 0, "Completion shown briefly")
        check(monitor.poll(now: now+20).state == "idle", "Completion expires")
        session.consume(record("session_meta", ["cwd": "/code/中文", "source": ["subagent": "worker"]], now), now: now)
        check(session.project == "Codex task" && session.ignored, "ASCII fallback and subagent exclusion")
        session.active = true; session.updated = now+20
        check(monitor.poll(now: now+21).active == 0, "Subagents excluded from task count")
        session.ignored = false; session.updated = now-4000
        check(monitor.poll(now: now+21).active == 0, "Abandoned old turns excluded")
        let waiting = RolloutSession(root.appendingPathComponent("waiting.jsonl"))
        waiting.active = true; waiting.started = now; waiting.updated = now; waiting.state = "waiting"
        let working = RolloutSession(root.appendingPathComponent("coding.jsonl"))
        working.active = true; working.started = now; working.updated = now+5; working.state = "coding"
        monitor.sessions[waiting.url] = waiting; monitor.sessions[working.url] = working
        check(monitor.poll(now: now+22).state == "waiting", "Waiting outranks newer coding task")
        let file = root.appendingPathComponent("partial.jsonl")
        let data = try JSONSerialization.data(withJSONObject: record("event_msg", ["type": "task_started"], now))
        try Data(data.prefix(data.count/2)).write(to: file)
        let partial = RolloutSession(file); partial.read(now: now)
        check(!partial.active, "Partial JSONL retained")
        let h = try FileHandle(forWritingTo: file); try h.seekToEnd(); try h.write(contentsOf: data.suffix(data.count-data.count/2) + Data([10])); try h.close()
        partial.read(now: now); check(partial.active, "Partial JSONL completes on append")
        try Data("{}\n".utf8).write(to: file); partial.read(now: now)
        check(!partial.active, "Truncation clears active session")
        var framing = ReplyStream()
        check(framing.feed(Data("{\"buddy_ack\":".utf8)).isEmpty, "Notification fragments buffered")
        check(framing.feed(Data("7}\n".utf8)).count == 1, "Notification reassembled")
        check(framing.feed(Data(repeating: 65, count: 5000) + Data("\n{\"buddy_ack\":8}\n".utf8)).count == 1, "Oversized line discarded and recovery")
        check(framing.feed(Data([65,0,66,10])).isEmpty, "NUL frame discarded")
        var receipt = ReceiptTracker(); receipt.sent(7)
        check(!receipt.receive(["buddy_ack": 6], now: now), "Unsent sequence rejected")
        check(!receipt.receive(["buddy_ack": true], now: now), "Boolean sequence rejected")
        check(!receipt.receive(["buddy_ack": "7"], now: now), "String sequence rejected")
        check(receipt.receive(["buddy_busy": 7], now: now) && receipt.busy && !receipt.confirmed(now: now), "Busy is not a successful state sync")
        receipt.sent(8); check(receipt.receive(["buddy_ack": 8], now: now+1), "Matching ACK")
        check(receipt.confirmed(now: now+2) && !receipt.confirmed(now: now+8), "ACK heartbeat freshness")
        let wire = try JSONSerialization.jsonObject(with: BuddyPacket(state: "thinking",active: 1,elapsed: 9,task: "demo").encoded(seq: 3)) as! [String: Any]
        check(Set(wire.keys) == Set(["v","state","active","elapsed","task","seq","theme","screenOffSeconds"]), "Wire payload contains metadata only")
        check(RolloutSession.classify("exec_command", "git diff") == "reviewing", "Review classification")
        check(RolloutSession.classify("exec_command", "./tools/validate.sh") == "building", "Build classification")
        var play = PetInteraction()
        let real = BuddyPacket(state: "coding", active: 2, elapsed: 12, task: "project")
        check(play.start(.curious, base: real, connected: true, now: now), "Play during work can be requested")
        var frame = play.current(base: real, connected: true, now: now+1)
        check(frame?.state == "thinking", "First interaction frame")
        let sent = play.packet(base: real, moment: frame)
        check(sent.active == 2 && sent.elapsed == 12 && sent.task == "project", "Play preserves real task metadata")
        frame = play.current(base: real, connected: true, now: now+2.1)
        check(frame?.state == "waving", "Sequence advances")
        check(play.current(base: real, connected: true, now: now+4.6) == nil, "Play expires without needing a menu open")
        check(play.packet(base: real, moment: nil) == real, "Expiry restores real task")
        _ = play.start(.nap, base: real, connected: true, now: now)
        var changed = real; changed.state = "waiting"
        check(play.current(base: changed, connected: true, now: now+1) == nil, "Waiting interrupts nap")
        check(!play.start(.wave, base: changed, connected: true, now: now), "Urgent status cannot be hidden")
        changed.state = "approval"
        check(!play.start(.praise, base: changed, connected: true, now: now), "Approval cannot be hidden")
        changed.state = "error"
        check(!play.start(.nap, base: changed, connected: true, now: now), "Error cannot be hidden")
        _ = play.start(.wave, base: real, connected: true, now: now)
        changed = real; changed.active = 3
        check(play.current(base: changed, connected: true, now: now+1) == nil, "New active task interrupts play")
        _ = play.start(.wave, base: real, connected: true, now: now)
        changed = real; changed.state = "testing"
        check(play.current(base: changed, connected: true, now: now+1) != nil, "Routine tool transitions do not cut play short")
        changed.state = "done"
        check(play.current(base: changed, connected: true, now: now+1) == nil, "Actual task completion interrupts play")
        _ = play.start(.wave, base: real, connected: false, now: now)
        check(play.current(base: real, connected: true, now: now+1) == nil, "Local preview never replays on connection")
        _ = play.touch(base: real, connected: true, now: now)
        _ = play.touch(base: real, connected: true, now: now+0.3)
        _ = play.touch(base: real, connected: true, now: now+0.6)
        check(play.current(base: real, connected: true, now: now+0.7)?.state == "done", "Repeated touches receive a different response")
        play.stop(); check(play.current(base: real, connected: true, now: now+1) == nil, "Manual return cancels sequence")
        let supported = Set(["waving", "done", "thinking", "sleeping", "searching", "received", "planning", "reviewing"])
        check(PetAction.scenes.flatMap { $0 }.allSatisfy { supported.contains($0.state) && $0.duration >= 2 }, "Scenes use existing firmware states with audible spacing")
        let customHome = root.appendingPathComponent("custom")
        let customSessions = customHome.appendingPathComponent("sessions")
        check(CodexLocation.sessions(in: customHome).path == customSessions.path, "Home selection resolves sessions child")
        check(CodexLocation.sessions(in: customSessions).path == customSessions.path, "Sessions selection is not nested twice")
        check(CodexLocation.resolve(saved: "/chosen/sessions", environment: ["CODEX_HOME": "/other"], home: root).path == "/chosen/sessions", "Saved selection overrides environment")
        check(CodexLocation.resolve(saved: nil, environment: ["CODEX_HOME": customHome.path], home: root).path == customSessions.path, "Custom CODEX_HOME supported")
        check(CodexLocation.resolve(saved: nil, environment: [:], home: root).path == root.appendingPathComponent(".codex/sessions").path, "Default source is user-local")
        check(CodexLocation.resolve(saved: nil, environment: ["CODEX_HOME": "relative"], home: root).path == root.appendingPathComponent(".codex/sessions").path, "Relative environment cannot depend on launch directory")
        let sourceMonitor = StateMonitor(root: customSessions, launched: now-1)
        check(sourceMonitor.poll(now: now).active == 0 && sourceMonitor.health == .missing, "Missing source differs from idle")
        try FileManager.default.createDirectory(at: customSessions, withIntermediateDirectories: true)
        _ = sourceMonitor.poll(now: now+6)
        check(sourceMonitor.health == .noRecentRecords, "Readable empty source has explicit guidance")
        let day = DateFormatter(); day.dateFormat = "yyyy/MM/dd"; day.timeZone = TimeZone(secondsFromGMT: 0)
        let recent = customSessions.appendingPathComponent(day.string(from: Date(timeIntervalSince1970: now)))
        try FileManager.default.createDirectory(at: recent, withIntermediateDirectories: true)
        var rollout = Data()
        for entry in [record("session_meta", ["cwd": "/code/custom-project"], now), record("event_msg", ["type": "task_started"], now)] {
            rollout.append(try JSONSerialization.data(withJSONObject: entry)); rollout.append(10)
        }
        try rollout.write(to: recent.appendingPathComponent("task.jsonl"))
        let found = sourceMonitor.poll(now: now+12)
        check(sourceMonitor.health == .ready && found.active == 1 && found.task == "custom-project", "Selected source reads real JSONL fixture")
        sourceMonitor.setRoot(root.appendingPathComponent("other-sessions"))
        let cleared = sourceMonitor.poll(now: now+13)
        check(cleared.active == 0 && cleared.state == "idle" && sourceMonitor.sessions.isEmpty, "Changing source discards old task state")
        let notFolder = root.appendingPathComponent("not-a-directory")
        try Data("test".utf8).write(to: notFolder)
        sourceMonitor.setRoot(notFolder); _ = sourceMonitor.poll(now: now+14)
        check(sourceMonitor.health == .unreadable, "File path is not a valid source directory")
        var text = TextReaction()
        check(text.observe("已有哈哈", caret: 4, now: 0) == nil, "Focus establishes baseline")
        check(text.observe("已有哈哈哈", caret: 5, now: 1) == nil, "Old keyword does not repeat")
        text.reset()
        _ = text.observe("", caret: 0, now: 2)
        check(text.observe("好困", caret: 2, now: 3) == .nap, "Chinese typed phrase reacts")
        check(text.observe("好困？？", caret: 4, now: 4) == nil, "Global cooldown avoids interruption")
        check(text.observe("好困？？", caret: 4, now: 20) == nil, "Cooldown does not replay stale text")
        check(text.observe("好困？？加油", caret: 6, now: 21) == .praise, "Different emotion after cooldown")
        text.reset(); _ = text.observe("为什么a", caret: 4, now: 40)
        check(text.observe("为什么", caret: 3, now: 41) == nil, "Deletion does not reveal a trigger")
        text.reset(); _ = text.observe("", caret: 0, now: 50)
        check(text.observe("你好", caret: 2, now: 51, allowed: false) == nil, "Urgent or manual interaction suppresses reactions")
        check(text.observe("你好", caret: 2, now: 65) == nil, "Suppressed input does not replay")
        text.reset(); _ = text.observe("ha", caret: 2, now: 70)
        check(text.observe("哈哈", caret: 2, now: 71) == .praise, "IME replacement can react")
        text.reset(); _ = text.observe("text", caret: 4, now: 90)
        check(text.observe("你好", caret: 40, now: 91) == nil, "Caret jumps do not react")
        var captionPacket = BuddyPacket()
        captionPacket.caption = String(repeating: "/", count: 576)
        let captionWire = captionPacket.encoded(seq: 1)
        check(captionWire.count < 1024, "Raster packet fits firmware stream")
        let decodedCaption = try JSONSerialization.jsonObject(with: captionWire) as? [String: Any]
        check(decodedCaption?["caption"] as? String == captionPacket.caption, "Raster survives packet encoding")
        print("State monitor and transport protocol: PASS (\(checks) checks)")
    }
}
