import Foundation
import CoreBluetooth
import IOKit
import IOKit.serial
import Darwin

struct USBDevice: Equatable { let path: String; let name: String }

final class USBLink {
    var status = "等待 USB 设备"
    var onReply: (([String: Any]) -> Void)?
    private(set) var path: String?
    private var fd: Int32 = -1
    private var source: DispatchSourceRead?
    private var pending = Data()
    private var stream = ReplyStream()
    private var lockFD: Int32 = -1

    static func devices() -> [USBDevice] {
        var iterator: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &iterator) == KERN_SUCCESS else { return [] }
        defer { IOObjectRelease(iterator) }
        var result: [USBDevice] = []
        while true {
            let service = IOIteratorNext(iterator); if service == 0 { break }
            defer { IOObjectRelease(service) }
            guard let port = IORegistryEntryCreateCFProperty(service, kIOCalloutDeviceKey as CFString, kCFAllocatorDefault, 0)?.takeRetainedValue() as? String,
                  let vid = IORegistryEntrySearchCFProperty(service, kIOServicePlane, "idVendor" as CFString, kCFAllocatorDefault, IOOptionBits(kIORegistryIterateRecursively | kIORegistryIterateParents)) as? NSNumber,
                  let pid = IORegistryEntrySearchCFProperty(service, kIOServicePlane, "idProduct" as CFString, kCFAllocatorDefault, IOOptionBits(kIORegistryIterateRecursively | kIORegistryIterateParents)) as? NSNumber,
                  vid.intValue == 0x303a, pid.intValue == 0x1001 else { continue }
            result.append(USBDevice(path: port, name: "AI Passport · " + URL(fileURLWithPath: port).lastPathComponent))
        }
        return result.sorted { $0.path < $1.path }
    }
    @discardableResult func connect(_ device: USBDevice) -> Bool {
        if path == device.path { return true }
        disconnect()
        let lockPath = "/tmp/codex-buddy-" + device.path.replacingOccurrences(of: "[^A-Za-z0-9]", with: "_", options: .regularExpression) + ".lock"
        lockFD = open(lockPath, O_CREAT | O_RDWR | O_NOFOLLOW, 0o600)
        guard lockFD >= 0, flock(lockFD, LOCK_EX | LOCK_NB) == 0 else {
            status = "USB 正被其他桥接程序使用"; releaseLock(); return false
        }
        fd = open(device.path, O_RDWR | O_NOCTTY | O_NONBLOCK)
        guard fd >= 0 else { status = "无法打开 USB，请检查连接"; releaseLock(); return false }
        var tty = termios()
        guard tcgetattr(fd, &tty) == 0 else { disconnect(); status = "无法读取 USB 配置"; return false }
        cfmakeraw(&tty); cfsetispeed(&tty, speed_t(B115200)); cfsetospeed(&tty, speed_t(B115200))
        tty.c_cflag |= tcflag_t(CLOCAL | CREAD); tty.c_cflag &= ~tcflag_t(HUPCL)
        guard tcsetattr(fd, TCSANOW, &tty) == 0 else { disconnect(); status = "无法设置 USB 连接"; return false }
        // No DTR/RTS toggling: opening the app must not reset the pet.
        path = device.path; status = "USB 已打开，等待设备确认"
        let descriptor = fd
        let readSource = DispatchSource.makeReadSource(fileDescriptor: descriptor, queue: .main)
        readSource.setEventHandler { [weak self] in self?.readAvailable(descriptor) }
        readSource.setCancelHandler { Darwin.close(descriptor) }
        source = readSource; readSource.resume()
        return true
    }
    private func readAvailable(_ descriptor: Int32) {
        guard fd == descriptor else { return }
        var bytes = [UInt8](repeating: 0, count: 4096)
        while true {
            let n = Darwin.read(descriptor, &bytes, bytes.count)
            if n > 0 { for reply in stream.feed(Data(bytes.prefix(n))) { onReply?(reply) } }
            else if n < 0 && errno == EAGAIN { return }
            else { disconnect(); status = "USB 已断开"; return }
        }
    }
    func send(_ data: Data) {
        guard fd >= 0 else { return }
        if pending.count + data.count > 8192 { disconnect(); status = "USB 写入超时"; return }
        pending.append(data); flush()
    }
    func flush() {
        guard fd >= 0, !pending.isEmpty else { return }
        let n = pending.withUnsafeBytes { Darwin.write(fd, $0.baseAddress, $0.count) }
        if n > 0 { pending.removeFirst(n) }
        else if n < 0 && errno != EAGAIN && errno != EINTR { disconnect(); status = "USB 写入失败" }
    }
    func disconnect() {
        if let source { source.cancel(); self.source = nil }
        else if fd >= 0 { Darwin.close(fd) }
        fd = -1; path = nil; pending.removeAll(); stream = ReplyStream(); releaseLock()
    }
    private func releaseLock() { if lockFD >= 0 { Darwin.close(lockFD); lockFD = -1 } }
    deinit { disconnect() }
}

final class BLELink: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    static let service = CBUUID(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    static let rxID = CBUUID(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    static let txID = CBUUID(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    private var central: CBCentralManager?
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private var chunks: [Data] = []
    private var writing = false
    private var stream = ReplyStream()
    private var lockFD: Int32 = -1
    private var attemptAt: TimeInterval = 0
    private var retryAt: TimeInterval = 0
    private var candidates: [UUID: CBPeripheral] = [:]
    private var seen: [UUID: TimeInterval] = [:]
    private(set) var ready = false
    private(set) var status = "蓝牙尚未启用"
    private(set) var enabled = false
    var selected: UUID?
    var onReply: (([String: Any]) -> Void)?
    var onChange: (() -> Void)?
    var devices: [(UUID, String)] { candidates.values.map { ($0.identifier, $0.name ?? "CodexBuddy") }.sorted { $0.0.uuidString < $1.0.uuidString } }

    func start() {
        guard !enabled else { return }
        lockFD = open("/tmp/codex-buddy-ble.lock", O_CREAT | O_RDWR | O_NOFOLLOW, 0o600)
        guard lockFD >= 0, flock(lockFD, LOCK_EX | LOCK_NB) == 0 else {
            if lockFD >= 0 { Darwin.close(lockFD); lockFD = -1 }
            status = "蓝牙正被其他桥接程序使用"; onChange?(); return
        }
        enabled = true; status = "正在准备蓝牙"
        if central == nil { central = CBCentralManager(delegate: self, queue: .main, options: [CBCentralManagerOptionShowPowerAlertKey: false]) }
        else { scan() }
    }
    func tick(now: TimeInterval) {
        guard enabled else { return }
        if let p = peripheral, !ready, now - attemptAt > 15 {
            central?.cancelPeripheralConnection(p); reset(); status = "蓝牙连接超时，正在重试"; retryAt = now + 3
        }
        if peripheral == nil, now >= retryAt, central?.state == .poweredOn { scan() }
    }
    func stop() {
        enabled = false; central?.stopScan()
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        reset(); status = "蓝牙待命"
        if lockFD >= 0 { Darwin.close(lockFD); lockFD = -1 }
    }
    private func reset() {
        ready = false; rx = nil; tx = nil; peripheral = nil
        chunks.removeAll(); writing = false; stream = ReplyStream(); onChange?()
    }
    private func scan() {
        guard enabled, central?.state == .poweredOn, peripheral == nil else { return }
        if central?.isScanning != true {
            candidates.removeAll(); seen.removeAll()
            central?.scanForPeripherals(withServices: [Self.service], options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
            attemptAt = Date().timeIntervalSince1970; status = "正在寻找 CodexBuddy…"; onChange?()
        }
        let now = Date().timeIntervalSince1970
        let fresh = candidates.values.filter { now - (seen[$0.identifier] ?? 0) < 8 }
        // Wait a short discovery window; never choose arbitrarily between devices.
        if now - attemptAt < 2 { return }
        let matches = fresh.filter { selected == nil || $0.identifier == selected }
        if matches.count == 1, let target = matches.first {
            peripheral = target; target.delegate = self; attemptAt = now
            central?.stopScan(); status = "正在连接 " + (target.name ?? "CodexBuddy")
            central?.connect(target, options: nil); onChange?()
        } else if matches.count > 1 { status = "发现多个设备，请在菜单中选择"; onChange?() }
        else if selected != nil { status = "等待所选蓝牙设备上线" }
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: if enabled { scan() }
        case .unauthorized: status = "请在系统设置中允许 Codex Buddy 使用蓝牙"; reset()
        case .poweredOff: status = "电脑蓝牙已关闭"; reset()
        case .unsupported: status = "此电脑不支持 BLE 蓝牙"; reset()
        default: status = "蓝牙正在准备"; reset()
        }
        onChange?()
    }
    func centralManager(_ central: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        candidates[p.identifier] = p; seen[p.identifier] = Date().timeIntervalSince1970
        onChange?()
    }
    func centralManager(_ central: CBCentralManager, didConnect p: CBPeripheral) {
        guard enabled, peripheral?.identifier == p.identifier else { central.cancelPeripheralConnection(p); return }
        status = "正在检查蓝牙服务"; p.discoverServices([Self.service]); onChange?()
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        guard peripheral?.identifier == p.identifier else { return }
        reset(); status = "蓝牙连接失败，自动重试"; retryAt = Date().timeIntervalSince1970 + 3
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        guard peripheral?.identifier == p.identifier else { return }
        reset(); status = "蓝牙已断开，自动重连"; retryAt = Date().timeIntervalSince1970 + 2
    }
    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard p.identifier == peripheral?.identifier else { return }
        guard error == nil, let service = p.services?.first(where: { $0.uuid == Self.service }) else { fail("蓝牙服务不可用"); return }
        p.discoverCharacteristics([Self.rxID, Self.txID], for: service)
    }
    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard p.identifier == peripheral?.identifier else { return }
        rx = service.characteristics?.first { $0.uuid == Self.rxID }
        tx = service.characteristics?.first { $0.uuid == Self.txID }
        guard error == nil, let rx, rx.properties.contains(.write), let tx, tx.properties.contains(.notify) else { fail("蓝牙收发服务不兼容"); return }
        p.setNotifyValue(true, for: tx)
    }
    func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard p.identifier == peripheral?.identifier, characteristic.uuid == Self.txID else { return }
        if error != nil || !characteristic.isNotifying { fail("无法订阅设备状态"); return }
        ready = true; status = "蓝牙已连接，等待设备确认"; onChange?()
    }
    func peripheral(_ p: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard p.identifier == peripheral?.identifier, characteristic.uuid == Self.txID, error == nil, let data = characteristic.value else { return }
        for reply in stream.feed(data) { onReply?(reply) }
    }
    @discardableResult func send(_ data: Data) -> Bool {
        guard ready, let peripheral, chunks.isEmpty, !writing else { return false }
        let size = max(1, min(512, peripheral.maximumWriteValueLength(for: .withResponse)))
        chunks = stride(from: 0, to: data.count, by: size).map { Data(data[$0..<min($0 + size, data.count)]) }
        writeNext(); return true
    }
    private func writeNext() {
        guard let peripheral, let rx, !chunks.isEmpty, !writing else { return }
        writing = true; peripheral.writeValue(chunks.removeFirst(), for: rx, type: .withResponse)
    }
    func peripheral(_ p: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard p.identifier == peripheral?.identifier, characteristic.uuid == Self.rxID else { return }
        if error != nil { fail("蓝牙写入失败，正在重连"); return }
        writing = false; writeNext()
    }
    func retry() { stop(); start() }
    private func fail(_ reason: String) {
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        reset(); status = reason; retryAt = Date().timeIntervalSince1970 + 3
    }
}
