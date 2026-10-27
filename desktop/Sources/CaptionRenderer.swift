import AppKit
import CoreText

/// Fixed 192×18 monochrome raster, sent at device resolution without scaling.
enum CaptionRenderer {
    static let width = 192, height = 18
    static func encode(_ text: String) -> String {
        guard !text.isEmpty else { return "" }
        var line = text.components(separatedBy: .newlines).last ?? ""
        guard !line.isEmpty else { return "" }
        // An explicit CJK face avoids the heavy system-font fallback at tiny sizes.
        let font = NSFont(name: "PingFangSC-Medium", size: 17) ?? NSFont.systemFont(ofSize: 17, weight: .regular)
        let style: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor.white]
        func typeset(_ value: String) -> CTLine {
            CTLineCreateWithAttributedString(NSAttributedString(string: value, attributes: style))
        }
        func fits(_ value: String) -> Bool {
            CTLineGetTypographicBounds(typeset(value), nil, nil, nil) <= Double(width)
        }
        if !fits(line) {
            // Remove complete grapheme clusters, including joined emoji.
            repeat { line.removeFirst() } while !line.isEmpty && !fits("…" + line)
            line = "…" + line
        }
        let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width, pixelsHigh: height,
                                   bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                                   isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: width * 4, bitsPerPixel: 32)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
        NSColor.black.setFill(); NSRect(x: 0, y: 0, width: width, height: height).fill()
        let context = NSGraphicsContext.current!.cgContext
        // Use grayscale coverage to choose each physical pixel. Direct non-AA
        // rasterization overfills dense CJK strokes; LCD subpixel smoothing is off.
        context.setShouldAntialias(true)
        context.setShouldSmoothFonts(false)
        context.setAllowsFontSmoothing(false)
        let glyphs = typeset(line)
        // A fixed font baseline prevents punctuation/emoji from moving the line.
        context.textMatrix = .identity
        context.textPosition = CGPoint(x: 0, y: max(0, ((CGFloat(height) - font.ascender + font.descender) / 2 - font.descender).rounded()))
        CTLineDraw(glyphs, context)
        NSGraphicsContext.restoreGraphicsState()
        var bits = Data(repeating: 0, count: width * height / 8)
        for y in 0..<height {
            for x in 0..<width {
                if let pixels = rep.bitmapData, pixels[y * rep.bytesPerRow + x * 4] >= 128 {
                    let index = y * width + x
                    bits[index / 8] |= UInt8(0x80 >> (index % 8))
                }
            }
        }
        return bits.base64EncodedString()
    }
}
