import AppKit
let directory = URL(fileURLWithPath: CommandLine.arguments[1]).appendingPathComponent("AppIcon.iconset")
try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let px = size * scale
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
        let d = CGFloat(px)
        let face = NSRect(x: d * 0.07, y: d * 0.07, width: d * 0.86, height: d * 0.86)
        NSColor(calibratedRed: 0.065, green: 0.09, blue: 0.11, alpha: 1).setFill()
        NSBezierPath(roundedRect: face, xRadius: d * 0.21, yRadius: d * 0.21).fill()
        NSColor(calibratedRed: 0.39, green: 0.85, blue: 0.90, alpha: 1).setFill()
        for x in [0.26, 0.57] {
            NSBezierPath(roundedRect: NSRect(x: d * x, y: d * 0.38, width: d * 0.17, height: d * 0.27), xRadius: d * 0.085, yRadius: d * 0.085).fill()
        }
        NSGraphicsContext.restoreGraphicsState()
        let name = "icon_\(size)x\(size)" + (scale == 2 ? "@2x" : "") + ".png"
        try bitmap.representation(using: .png, properties: [:])!.write(to: directory.appendingPathComponent(name))
    }
}
