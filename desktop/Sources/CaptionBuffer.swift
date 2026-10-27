import Foundation

/// Coalesce edits without letting continuous typing starve the display.
struct CaptionBuffer {
    private var previous: String?
    private var caret = 0
    private var pending: String?
    private var pendingSince: TimeInterval = 0
    private var editedAt: TimeInterval = 0
    private(set) var text = ""

    mutating func reset() { self = CaptionBuffer() }
    mutating func hold(now: TimeInterval) {
        pending = nil
        if now - editedAt >= 5 { text = "" }
    }
    mutating func observe(_ tail: String, caret nextCaret: Int, now: TimeInterval) {
        defer { previous = tail; caret = nextCaret }
        guard let old = previous else { return }
        if old != tail {
            let delta = nextCaret - caret
            let edited = delta == tail.utf16.count - old.utf16.count ||
                (old.utf16.count == 64 && tail.utf16.count == 64 && delta > 0)
            guard edited else { hold(now: now); return }
            if pending == nil { pendingSince = now }
            pending = tail; editedAt = now
        } else if nextCaret != caret {
            hold(now: now); return
        }
        if let candidate = pending, now - editedAt >= 0.3 || now - pendingSince >= 0.9 {
            text = candidate; pending = nil
        }
        if now - editedAt >= 5 { text = "" }
    }
}
