import Foundation

struct PetMoment {
    let state: String
    let title: String
    let duration: TimeInterval
}

enum PetAction: String, CaseIterable {
    case wave, praise, curious, nap, theatre
    var title: String {
        switch self {
        case .wave: return "打个招呼"
        case .praise: return "夸夸它"
        case .curious: return "逗逗它"
        case .nap: return "打个盹"
        case .theatre: return "随机小剧场"
        }
    }
    var moments: [PetMoment] {
        switch self {
        case .wave: return [.init(state: "waving", title: "看到你啦", duration: 3)]
        case .praise: return [.init(state: "done", title: "收到夸奖，开心一下", duration: 4)]
        case .curious: return [.init(state: "thinking", title: "嗯？你在叫我？", duration: 2), .init(state: "waving", title: "原来是你呀", duration: 2.5)]
        case .nap: return [.init(state: "sleeping", title: "眯一会儿，任务来了就醒", duration: 8)]
        case .theatre: return Self.scenes.randomElement()!
        }
    }
    static let scenes: [[PetMoment]] = [
        [.init(state: "searching", title: "发现一点动静", duration: 2), .init(state: "thinking", title: "让我看看……", duration: 2.5), .init(state: "waving", title: "找到你了", duration: 3)],
        [.init(state: "sleeping", title: "假装睡着", duration: 2.5), .init(state: "received", title: "被你发现了", duration: 2), .init(state: "done", title: "偷偷开心", duration: 3)],
        [.init(state: "planning", title: "准备一个小惊喜", duration: 2.5), .init(state: "reviewing", title: "再确认一下", duration: 2), .init(state: "waving", title: "今天也一起加油", duration: 3)]
    ]
}

/// Temporary visual play, never a replacement for the real task record.
struct PetInteraction {
    private(set) var moments: [PetMoment] = []
    private var started: TimeInterval = 0
    private var base = BuddyPacket()
    private var wasConnected = false
    private var lastTouch: TimeInterval = -.infinity
    private var touches = 0
    static func urgent(_ packet: BuddyPacket) -> Bool {
        ["waiting", "approval", "error"].contains(packet.state)
    }
    mutating func start(_ action: PetAction, base: BuddyPacket, connected: Bool, now: TimeInterval) -> Bool {
        guard !Self.urgent(base) else { stop(); return false }
        moments = action.moments; started = now; self.base = base; wasConnected = connected
        return true
    }
    mutating func touch(base: BuddyPacket, connected: Bool, now: TimeInterval) -> Bool {
        touches = now - lastTouch < 1.4 ? touches + 1 : 1; lastTouch = now
        let action: PetAction = touches % 3 == 0 ? .praise : touches % 2 == 0 ? .curious : .wave
        return start(action, base: base, connected: connected, now: now)
    }
    mutating func stop() { moments.removeAll() }
    mutating func current(base: BuddyPacket, connected: Bool, now: TimeInterval) -> PetMoment? {
        guard !moments.isEmpty else { return nil }
        // Routine tool transitions may advance; new tasks and terminal/urgent events take priority.
        let taskEnded = ["done", "cancelled"].contains(base.state) && self.base.state != base.state
        let taskStarted = base.state == "received" && self.base.state != base.state
        guard !Self.urgent(base), !taskEnded, !taskStarted, self.base.active == base.active,
              self.base.task == base.task, connected == wasConnected else { stop(); return nil }
        var elapsed = max(0, now - started)
        for moment in moments {
            if elapsed < moment.duration { return moment }
            elapsed -= moment.duration
        }
        stop(); return nil
    }
    func packet(base: BuddyPacket, moment: PetMoment?) -> BuddyPacket {
        var result = base
        if let moment { result.state = moment.state }
        return result
    }
}
