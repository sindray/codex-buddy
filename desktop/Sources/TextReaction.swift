import Foundation

/// Only the bounded caret tail is held in memory. No text leaves this matcher.
struct TextReaction {
    private var previous: String?
    private var previousCaret = 0
    private var lastReaction: TimeInterval = -.infinity
    let cooldown: TimeInterval = 12

    mutating func reset() { previous = nil; previousCaret = 0 }

    mutating func observe(_ tail: String, caret: Int, now: TimeInterval, allowed: Bool = true) -> PetAction? {
        let text = String(tail.suffix(64))
        defer { previous = text; previousCaret = caret }
        guard let old = previous, text != old else { return nil }
        // A caret move is not typing. Deletion must not uncover an old trigger.
        let change = caret - previousCaret
        guard change == text.utf16.count - old.utf16.count ||
                (old.utf16.count == 64 && text.utf16.count == 64 && change > 0),
              !old.hasPrefix(text) else { return nil }
        let normalized = text.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        let rules: [(PetAction, [String])] = [
            (.nap, ["好困", "困了", "晚安", "累了", "休息一下"]),
            (.praise, ["哈哈", "嘿嘿", "笑死", "加油", "搞定", "成功了", "太棒了"]),
            (.curious, ["？？", "??", "为什么", "怎么回事"]),
            (.wave, ["你好", "嗨buddy", "hi buddy", "hello buddy"])
        ]
        guard allowed, now - lastReaction >= cooldown else { return nil }
        for (action, phrases) in rules {
            if phrases.contains(where: { normalized.hasSuffix($0) && !old.lowercased().trimmingCharacters(in: .whitespacesAndNewlines).hasSuffix($0) }) {
                lastReaction = now
                return action
            }
        }
        return nil
    }
}
