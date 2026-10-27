import Foundation
import CoreFoundation

struct BuddyPacket: Equatable {
    var state = "idle"
    var active = 0
    var elapsed = 0
    var task = "Codex task"
    var caption = ""
    var theme = "dark"
    var screenOffSeconds = 300
    func encoded(seq: Int) -> Data {
        var value: [String: Any] = ["v": 1, "state": state, "active": active,
                                    "elapsed": elapsed, "task": task, "seq": seq, "theme": theme, "screenOffSeconds": screenOffSeconds]
        if !caption.isEmpty { value["caption"] = caption }
        var data = (try? JSONSerialization.data(withJSONObject: value, options: [.sortedKeys, .withoutEscapingSlashes])) ?? Data()
        data.append(10)
        return data
    }
}

final class RolloutSession {
    let url: URL
    var offset: UInt64 = 0
    var inode: UInt64 = 0
    var buffer = Data()
    var project = "Codex task"
    var state = "idle"
    var active = false
    var started: TimeInterval = 0
    var updated: TimeInterval = 0
    var turn = ""
    var ignored = false
    var pending: [(String, String)] = []
    private static let iso = ISO8601DateFormatter()
    private static let fractional: ISO8601DateFormatter = {
        let f = ISO8601DateFormatter(); f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]; return f
    }()
    init(_ url: URL) { self.url = url }
    static func classify(_ name: String, _ raw: String) -> String {
        let name = name.lowercased()
        if name.contains("request_user_input") || raw.contains("request_user_input") { return "waiting" }
        if ["apply_patch", "write_file", "edit_file"].contains(where: name.contains) || raw.contains("tools.apply_patch(") { return "coding" }
        if ["search", "web", "browse"].contains(where: name.contains) || raw.contains("tools.web__run(") { return "searching" }
        if name.contains("update_plan") { return "planning" }
        let patterns = [
            (#"\b(?:pytest|ctest|unittest|vitest)\b|(?:npm|pnpm) (?:run )?test\b"#, "testing"),
            (#"\bidf\.py\b[^\n]{0,80}\b(?:build|reconfigure|merge-bin)\b|(?:npm|pnpm) run build|validate\.sh"#, "building"),
            (#"\bgit (?:diff|status)\b"#, "reviewing"),
            (#"\b(?:rg|cat|sed|head|tail|ls|find)\b"#, "reading")]
        return patterns.first { raw.range(of: $0.0, options: .regularExpression) != nil }?.1 ?? "coding"
    }
    func consume(_ value: [String: Any], now: TimeInterval) {
        guard let p = value["payload"] as? [String: Any] else { return }
        let kind = value["type"] as? String ?? ""
        let timestamp = value["timestamp"] as? String ?? ""
        let stamp = (Self.fractional.date(from: timestamp) ?? Self.iso.date(from: timestamp))?.timeIntervalSince1970 ?? now
        if kind == "session_meta" {
            ignored = (p["source"] as? [String: Any])?["subagent"] != nil
            let name = URL(fileURLWithPath: p["cwd"] as? String ?? "").lastPathComponent
            project = !name.isEmpty && name.utf8.count <= 32 && name.utf8.allSatisfy { $0 >= 32 && $0 <= 126 } ? name : "Codex task"
            return
        }
        guard !ignored else { return }
        let event = p["type"] as? String ?? ""
        if kind == "event_msg" {
            if event == "task_started" {
                active = true; started = stamp; turn = p["turn_id"] as? String ?? ""
                state = "received"; updated = stamp; pending.removeAll()
            } else if event == "task_complete" || event == "turn_aborted" {
                let finished = p["turn_id"] as? String ?? ""
                if !finished.isEmpty && !turn.isEmpty && finished != turn { return }
                active = false
                let error = p["error"]
                let hasError = error != nil && !(error is NSNull) && (error as? Bool != false) && (error as? String != "")
                state = event == "turn_aborted" ? "cancelled" : hasError ? "error" : "done"
                updated = stamp; pending.removeAll()
            }
        } else if kind == "response_item" && active {
            let id = p["call_id"] as? String ?? ""
            if event == "function_call" || event == "custom_tool_call" {
                let next = Self.classify(p["name"] as? String ?? "", p["arguments"] as? String ?? p["input"] as? String ?? "")
                pending.removeAll { $0.0 == id }; pending.append((id, next))
                state = next; updated = stamp
            } else if event == "function_call_output" || event == "custom_tool_call_output" {
                pending.removeAll { $0.0 == id }; state = pending.last?.1 ?? "thinking"; updated = stamp
            } else if event == "reasoning" { state = pending.last?.1 ?? "thinking"; updated = stamp }
        }
    }
    func read(now: TimeInterval) {
        guard let info = try? FileManager.default.attributesOfItem(atPath: url.path),
              let size = (info[.size] as? NSNumber)?.uint64Value,
              let node = (info[.systemFileNumber] as? NSNumber)?.uint64Value else { return }
        if node != inode || size < offset {
            offset = 0; buffer.removeAll(); inode = node
            active = false; pending.removeAll(); updated = 0; started = 0; state = "idle"; ignored = false
        }
        guard let file = try? FileHandle(forReadingFrom: url) else { return }
        defer { try? file.close() }
        do {
            try file.seek(toOffset: offset)
            let data = try file.read(upToCount: 4 * 1024 * 1024) ?? Data()
            offset += UInt64(data.count); buffer.append(data)
            // Split once per bounded read, avoiding repeated removal from a large Data.
            let lines = buffer.split(separator: 10, omittingEmptySubsequences: false)
            buffer = Data(lines.last ?? Data.SubSequence())
            for line in lines.dropLast() {
                if let record = (try? JSONSerialization.jsonObject(with: Data(line))) as? [String: Any] { consume(record, now: now) }
            }
            if buffer.count > 8 * 1024 * 1024 { buffer.removeAll() }
        } catch { return }
    }
}

enum SourceHealth: String {
    case checking, missing, unreadable, noRecentRecords, ready
    var message: String {
        switch self {
        case .checking: return "正在检测 Codex 目录…"
        case .missing: return "未找到 Codex 记录目录，请运行 Codex 或选择目录"
        case .unreadable: return "无法读取 Codex 目录，请重新选择可访问的目录"
        case .noRecentRecords: return "目录可访问，近两天暂无记录；请在 Codex 发起任务"
        case .ready: return "已发现近期记录 · 本机 Codex"
        }
    }
}

struct CodexLocation {
    static func sessions(in folder: URL) -> URL {
        let folder = folder.standardizedFileURL
        return folder.lastPathComponent == "sessions" ? folder : folder.appendingPathComponent("sessions", isDirectory: true)
    }
    static func resolve(saved: String?, environment: [String: String] = ProcessInfo.processInfo.environment,
                        home: URL = FileManager.default.homeDirectoryForCurrentUser) -> URL {
        if let saved, !saved.isEmpty { return URL(fileURLWithPath: saved).standardizedFileURL }
        if let value = environment["CODEX_HOME"], !value.isEmpty {
            let path = (value as NSString).expandingTildeInPath
            if path.hasPrefix("/") { return sessions(in: URL(fileURLWithPath: path)) }
        }
        return home.appendingPathComponent(".codex/sessions", isDirectory: true)
    }
}

final class StateMonitor {
    private(set) var root: URL
    let launched: TimeInterval
    var sessions: [URL: RolloutSession] = [:]
    var lastScan: TimeInterval = 0
    var health: SourceHealth = .checking
    var readable: Bool { health == .ready || health == .noRecentRecords }
    var projectFilter: String?
    init(root: URL = CodexLocation.resolve(saved: nil), launched: TimeInterval = Date().timeIntervalSince1970) {
        self.root = root; self.launched = launched
    }
    func setRoot(_ root: URL) {
        self.root = root; sessions.removeAll(); lastScan = 0; health = .checking
    }
    func poll(now: TimeInterval = Date().timeIntervalSince1970) -> BuddyPacket {
        if now - lastScan >= 5 {
            let f = DateFormatter(); f.dateFormat = "yyyy/MM/dd"; f.timeZone = TimeZone(secondsFromGMT: 0)
            var isDirectory: ObjCBool = false
            let exists = FileManager.default.fileExists(atPath: root.path, isDirectory: &isDirectory)
            health = !exists ? .missing : .unreadable
            guard exists, isDirectory.boolValue else { sessions.removeAll(); lastScan = now; return BuddyPacket() }
            do { _ = try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) }
            catch { sessions.removeAll(); lastScan = now; return BuddyPacket() }
            health = .noRecentRecords
            var recentFiles = Set<URL>()
            for days in 0...1 {
                let folder = root.appendingPathComponent(f.string(from: Date(timeIntervalSince1970: now - Double(days) * 86400)))
                for url in (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)) ?? [] where url.pathExtension == "jsonl" {
                    recentFiles.insert(url)
                    if sessions[url] == nil { sessions[url] = RolloutSession(url) }
                }
            }
            if !recentFiles.isEmpty { health = .ready }
            lastScan = now
        }
        sessions.values.forEach { $0.read(now: now) }
        let items = sessions.values.filter { !$0.ignored && (projectFilter == nil || $0.project == projectFilter) }
        let active = items.filter { $0.active && now - $0.updated < 3600 }
        let priority = ["waiting": 5, "approval": 6, "error": 4]
        var selected = active.max {
            let l = priority[$0.state] ?? 0, r = priority[$1.state] ?? 0
            return l == r ? $0.updated < $1.updated : l < r
        }
        if active.isEmpty {
            selected = items.filter { !$0.active && $0.updated >= launched }.max { $0.updated < $1.updated }
            if let s = selected, s.state != "error" && now - s.updated >= 12 { selected = nil }
        }
        var state = selected?.state ?? "idle"
        if state == "received", let s = selected, now - s.started > 3 { state = "thinking" }
        return BuddyPacket(state: state, active: min(active.count, 99),
                           elapsed: active.isEmpty ? 0 : min(604800, Int(max(0, now - (selected?.started ?? now)))),
                           task: selected?.project ?? "Codex task")
    }
}

/// Bounded newline framing shared by both transports. A bad frame cannot refresh an ACK timer.
struct ReplyStream {
    var buffer = Data()
    var overflow = false
    mutating func feed(_ data: Data) -> [[String: Any]] {
        var lines: [[String: Any]] = []
        for byte in data {
            if byte == 10 {
                if !overflow, let value = (try? JSONSerialization.jsonObject(with: buffer)) as? [String: Any] { lines.append(value) }
                buffer.removeAll(keepingCapacity: true); overflow = false
            } else if byte != 13 {
                if buffer.count >= 4096 || byte == 0 { overflow = true }
                else { buffer.append(byte) }
            }
        }
        return lines
    }
}

struct ReceiptTracker {
    var pending: Set<Int> = []
    var lastAccepted: TimeInterval = 0
    var lastResponse: TimeInterval = 0
    var busy = false
    mutating func sent(_ seq: Int) { pending.insert(seq); if pending.count > 32, let min = pending.min() { pending.remove(min) } }
    @discardableResult mutating func receive(_ item: [String: Any], now: TimeInterval) -> Bool {
        guard let raw = item["buddy_ack"] ?? item["buddy_busy"], !(raw is String),
              let n = raw as? NSNumber, CFGetTypeID(n) != CFBooleanGetTypeID(),
              n.doubleValue.rounded(.towardZero) == n.doubleValue, pending.contains(n.intValue) else { return false }
        pending.remove(n.intValue); lastResponse = now
        busy = item["buddy_ack"] == nil
        if !busy { lastAccepted = now }
        return true
    }
    func confirmed(now: TimeInterval) -> Bool { lastAccepted > 0 && now - lastAccepted < 6 }
}
