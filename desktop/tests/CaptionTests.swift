import AppKit

@main struct CaptionTests {
    static func main() throws {
        precondition(CaptionRenderer.encode("") == "")
        var buffer = CaptionBuffer()
        buffer.observe("", caret: 0, now: 0)
        buffer.observe("你", caret: 1, now: 0.1)
        precondition(buffer.text.isEmpty)
        buffer.observe("你好", caret: 2, now: 0.2)
        buffer.observe("你好", caret: 2, now: 0.6)
        precondition(buffer.text == "你好")
        buffer.hold(now: 0.7)
        precondition(buffer.text == "你好")
        buffer.observe("你", caret: 1, now: 0.8)
        buffer.observe("你", caret: 1, now: 1.2)
        precondition(buffer.text == "你")
        // A caret move inside unchanged-length text must not become new input.
        buffer.observe("好", caret: 0, now: 1.3)
        buffer.observe("好", caret: 0, now: 2)
        precondition(buffer.text == "你")
        buffer.hold(now: 7)
        precondition(buffer.text.isEmpty)
        buffer.reset(); buffer.observe("", caret: 0, now: 0)
        for i in 1...7 { buffer.observe(String(repeating: "a", count: i), caret: i, now: Double(i) * 0.2) }
        precondition(!buffer.text.isEmpty) // Continuous typing cannot starve updates.
        buffer.reset(); buffer.observe("existing", caret: 8, now: 0)
        precondition(buffer.text.isEmpty) // Focus change never exposes existing text.
        precondition(CaptionRenderer.encode("你好\n") == "")
        let phrases = ["你好，今天也要加油！", "Hello Buddy ??", "笑死😂", String(repeating: "这是很长的中文", count: 12)]
        for phrase in phrases {
            let encoded = CaptionRenderer.encode(phrase)
            let bits = Data(base64Encoded: encoded)!
            precondition(bits.count == 432)
            precondition(bits.contains(where: { $0 != 0 }))
        }
        // Punctuation/emoji must not move the existing letters up or down.
        func ink(_ data: Data, x: Int, y: Int) -> Bool {
            let i = y * CaptionRenderer.width + x
            return data[i / 8] & UInt8(0x80 >> (i % 8)) != 0
        }
        let prefix = Data(base64Encoded: CaptionRenderer.encode("你好"))!
        for suffix in ["!", "，", "😂", "👩🏽‍💻"] {
            let extended = Data(base64Encoded: CaptionRenderer.encode("你好" + suffix))!
            for y in 0..<CaptionRenderer.height { for x in 0..<26 {
                precondition(ink(prefix, x: x, y: y) == ink(extended, x: x, y: y))
            } }
        }
        // Overflow retains an explicit ellipsis and complete ending graphemes.
        let overflow = "…" + String(repeating: "很长", count: 20) + "完成👩🏽‍💻"
        precondition(CaptionRenderer.encode(overflow) == CaptionRenderer.encode(String(repeating: "中文", count: 40) + overflow))
        let bits = Data(base64Encoded: CaptionRenderer.encode(phrases[0]))!
        let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 192, pixelsHigh: 18, bitsPerSample: 8, samplesPerPixel: 3, hasAlpha: false, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 576, bitsPerPixel: 24)!
        for y in 0..<18 { for x in 0..<192 {
            let index = y*192+x
            let on = bits[index/8] & UInt8(0x80 >> (index%8)) != 0
            let offset = y * rep.bytesPerRow + x * 3
            for channel in 0..<3 { rep.bitmapData![offset + channel] = on ? 255 : 0 }
        } }
        try rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
        print("Caption raster: PASS (Chinese, English, emoji, overflow, fixed size, stable edits, selection hold, caret moves, expiry)")
    }
}
