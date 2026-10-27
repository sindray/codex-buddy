import AppKit
import ApplicationServices
import Carbon

/// Reads at most 64 UTF-16 units from a supported focused editor; never reads AXValue.
final class TextInputMonitor {
    private var element: AXUIElement?
    private var process: pid_t?
    private var reaction = TextReaction()
    private var captions = CaptionBuffer()
    var subtitleText: String { captions.text }
    private(set) var status = "disabled"
    private var hasSample = false
    static var trusted: Bool { AXIsProcessTrusted() }
    static func requestPermission() {
        _ = AXIsProcessTrustedWithOptions([kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary)
    }
    func reset(_ reason: String = "reset") { element = nil; process = nil; reaction.reset(); captions.reset(); hasSample = false; status = reason }
    private func attribute(_ element: AXUIElement, _ name: String) -> CFTypeRef? {
        var value: CFTypeRef?
        guard AXUIElementCopyAttributeValue(element, name as CFString, &value) == .success else { return nil }
        return value
    }
    func poll(enabled: Bool, allowed: Bool, now: TimeInterval) -> PetAction? {
        guard enabled else { reset("disabled"); return nil }
        guard Self.trusted else { reset("permissionRequired"); return nil }
        guard !IsSecureEventInputEnabled() else { reset("secureInput"); return nil }
        guard let app = NSWorkspace.shared.frontmostApplication,
              app.processIdentifier != ProcessInfo.processInfo.processIdentifier else { reset("noExternalApp"); return nil }
        let target = AXUIElementCreateApplication(app.processIdentifier)
        AXUIElementSetMessagingTimeout(target, 0.05)
        guard let raw = attribute(target, kAXFocusedUIElementAttribute), CFGetTypeID(raw) == AXUIElementGetTypeID() else { reset("unsupportedEditor"); return nil }
        let focused = unsafeBitCast(raw, to: AXUIElement.self)
        AXUIElementSetMessagingTimeout(focused, 0.05)
        guard let role = attribute(focused, kAXRoleAttribute) as? String,
              [kAXTextFieldRole, kAXTextAreaRole, kAXComboBoxRole].contains(role),
              (attribute(focused, kAXSubroleAttribute) as? String) != kAXSecureTextFieldSubrole,
              let rawRange = attribute(focused, kAXSelectedTextRangeAttribute), CFGetTypeID(rawRange) == AXValueGetTypeID() else { reset("unsupportedRange"); return nil }
        var selected = CFRange()
        guard AXValueGetValue(unsafeBitCast(rawRange, to: AXValue.self), .cfRange, &selected),
              selected.location >= 0 else { reset("unsupportedRange"); return nil }
        if process != app.processIdentifier || element.map({ !CFEqual($0, focused) }) != false {
            reaction.reset(); captions.reset(); hasSample = false
        }
        process = app.processIdentifier; element = focused
        // Selection and many input-method composition ranges are transient.
        guard selected.length == 0 else { captions.hold(now: now); status = "selection"; return nil }
        var range = CFRange(location: max(0, selected.location - 64), length: min(64, selected.location))
        guard let parameter = AXValueCreate(.cfRange, &range) else { reset("unsupportedRange"); return nil }
        var result: CFTypeRef?
        guard AXUIElementCopyParameterizedAttributeValue(focused, kAXStringForRangeParameterizedAttribute as CFString, parameter, &result) == .success,
              let tail = result as? String, tail.utf16.count <= 64 else { reset("unsupportedRange"); return nil }
        status = hasSample ? "monitoring" : "baseline"; hasSample = true
        captions.observe(tail, caret: selected.location, now: now)
        return reaction.observe(tail, caret: selected.location, now: now, allowed: allowed)
    }
}
