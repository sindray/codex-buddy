using System.IO.Ports;
using System.Management;
using System.Text.RegularExpressions;
using CodexBuddy.Core;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage.Streams;

namespace CodexBuddy.Windows;

record DeviceChoice(string Id, string Name);

sealed class UsbLink(Action<Action> dispatch) : IDisposable
{
    SerialPort? port;
    ReplyStream stream = new();
    public Action<System.Text.Json.JsonElement>? Reply;
    public string? Id => port?.PortName;
    public bool Open => port?.IsOpen == true;
    public static DeviceChoice[] Discover()
    {
        List<DeviceChoice> result = [];
        using var query = new ManagementObjectSearcher("SELECT Name, PNPDeviceID FROM Win32_PnPEntity WHERE PNPDeviceID LIKE '%VID_303A&PID_1001%'");
        using var rows = query.Get();
        foreach (ManagementBaseObject row in rows)
        {
            using (row)
            {
                string name = row["Name"]?.ToString() ?? "";
                var match = Regex.Match(name, @"\((COM\d+)\)", RegexOptions.IgnoreCase);
                if (match.Success) result.Add(new(match.Groups[1].Value, "AI Passport · " + match.Groups[1].Value));
            }
        }
        return result.DistinctBy(d => d.Id).OrderBy(d => d.Id).ToArray();
    }
    public void Connect(string id)
    {
        Dispose(); stream = new();
        var candidate = new SerialPort(id, 115200) { DtrEnable = false, RtsEnable = false, ReadTimeout = 500, WriteTimeout = 1000 };
        candidate.DataReceived += Read;
        try { candidate.Open(); port = candidate; }
        catch { candidate.Dispose(); throw; }
    }
    void Read(object sender, SerialDataReceivedEventArgs args)
    {
        var source = (SerialPort)sender;
        try
        {
            int available = Math.Min(source.BytesToRead, 4096); if (available <= 0) return;
            byte[] bytes = new byte[available]; int count = source.Read(bytes, 0, bytes.Length);
            dispatch(() => { if (port != source) return; foreach (var reply in stream.Feed(bytes.AsSpan(0, count))) Reply?.Invoke(reply); });
        }
        catch (Exception e) when (e is IOException or InvalidOperationException or UnauthorizedAccessException or TimeoutException) { }
    }
    public Task Send(byte[] data) => Task.Run(() => { var p = port ?? throw new IOException("USB 已断开"); p.Write(data, 0, data.Length); });
    public void Dispose() { var old = port; port = null; if (old != null) { old.DataReceived -= Read; old.Dispose(); } stream = new(); }
}

sealed class BleLink(Action<Action> dispatch) : IDisposable
{
    public static readonly Guid ServiceId = new("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    static readonly Guid RxId = new("6e400002-b5a3-f393-e0a9-e50e24dcca9e"), TxId = new("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    readonly BluetoothLEAdvertisementWatcher watcher = new() { ScanningMode = BluetoothLEScanningMode.Active };
    readonly Dictionary<ulong, double> seen = [];
    BluetoothLEDevice? device;
    GattDeviceService? service;
    GattCharacteristic? rx, tx;
    ReplyStream stream = new();
    int generation;
    public Action<System.Text.Json.JsonElement>? Reply;
    public string ScanStatus { get; private set; } = "蓝牙尚未扫描";
    public bool Ready => device?.ConnectionStatus == BluetoothConnectionStatus.Connected && rx != null && tx != null;
    public string? Id { get; private set; }
    public DeviceChoice[] Devices(double now) => seen.Where(x => now - x.Value < 20).OrderBy(x => x.Key).Select(x => new DeviceChoice(x.Key.ToString(), $"CodexBuddy · {x.Key:X12}")).ToArray();
    public void StartScan()
    {
        if (watcher.Status == BluetoothLEAdvertisementWatcherStatus.Started) return;
        watcher.Received -= Advertisement; watcher.Received += Advertisement;
        watcher.Stopped -= Stopped; watcher.Stopped += Stopped;
        watcher.AdvertisementFilter.Advertisement.ServiceUuids.Clear();
        watcher.AdvertisementFilter.Advertisement.ServiceUuids.Add(ServiceId);
        try { watcher.Start(); ScanStatus = "正在查找蓝牙设备"; }
        catch (Exception e) when (e is not OutOfMemoryException) { ScanStatus = "无法扫描蓝牙，请检查适配器及系统权限"; }
    }
    void Stopped(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementWatcherStoppedEventArgs args) => dispatch(() => { if (args.Error != BluetoothError.Success) ScanStatus = "蓝牙扫描停止：" + args.Error; });
    void Advertisement(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args) => dispatch(() => seen[args.BluetoothAddress] = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() / 1000d);
    public async Task Connect(string id)
    {
        Disconnect(); int epoch = generation;
        BluetoothLEDevice? candidate = null; GattDeviceService? found = null; bool owned = false;
        try
        {
            // Called on the UI thread so Windows can show device access consent.
            candidate = await BluetoothLEDevice.FromBluetoothAddressAsync(ulong.Parse(id));
            if (candidate == null) throw new IOException("蓝牙设备不可访问，请检查系统蓝牙权限");
            if (epoch != generation) throw new OperationCanceledException();
            var services = await candidate.GetGattServicesForUuidAsync(ServiceId, BluetoothCacheMode.Uncached);
            if (services.Status != GattCommunicationStatus.Success || services.Services.Count == 0) throw new IOException("蓝牙服务不可用，设备可能已连接其他电脑");
            found = services.Services[0];
            foreach (var extra in services.Services.Skip(1)) extra.Dispose();
            if (epoch != generation) throw new OperationCanceledException();
            var chars = await found.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
            if (chars.Status != GattCommunicationStatus.Success) throw new IOException("无法读取蓝牙收发服务");
            var receive = chars.Characteristics.FirstOrDefault(c => c.Uuid == RxId && c.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Write));
            var notify = chars.Characteristics.FirstOrDefault(c => c.Uuid == TxId && c.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Notify));
            if (receive == null || notify == null) throw new IOException("蓝牙收发服务不兼容");
            if (epoch != generation) throw new OperationCanceledException();
            notify.ValueChanged += Notification;
            device = candidate; service = found; rx = receive; tx = notify; Id = id; stream = new(); owned = true;
            var status = await notify.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify);
            if (epoch != generation) throw new OperationCanceledException();
            if (status != GattCommunicationStatus.Success) throw new IOException("无法订阅设备回执");
        }
        catch { if (epoch == generation) Disconnect(); throw; }
        finally { if (!owned) { found?.Dispose(); candidate?.Dispose(); } }
    }
    void Notification(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        using var reader = DataReader.FromBuffer(args.CharacteristicValue); byte[] bytes = new byte[reader.UnconsumedBufferLength]; reader.ReadBytes(bytes);
        dispatch(() => { if (sender != tx) return; foreach (var reply in stream.Feed(bytes)) Reply?.Invoke(reply); });
    }
    public async Task Send(byte[] data)
    {
        var target = rx ?? throw new IOException("蓝牙已断开"); int epoch = generation;
        // Conservative 20-byte payload works with the minimum ATT MTU; preserve newline framing across writes.
        for (int offset = 0; offset < data.Length; offset += 20)
        {
            if (epoch != generation) throw new OperationCanceledException();
            using var writer = new DataWriter(); writer.WriteBytes(data.AsSpan(offset, Math.Min(20, data.Length - offset)).ToArray());
            var status = await target.WriteValueAsync(writer.DetachBuffer(), GattWriteOption.WriteWithResponse);
            if (status != GattCommunicationStatus.Success) throw new IOException("蓝牙写入失败");
        }
    }
    public void Disconnect()
    {
        generation++;
        if (tx != null) tx.ValueChanged -= Notification;
        rx = tx = null; Id = null; stream = new(); service?.Dispose(); service = null; device?.Dispose(); device = null;
    }
    public void Stop() { if (watcher.Status == BluetoothLEAdvertisementWatcherStatus.Started) watcher.Stop(); seen.Clear(); Disconnect(); }
    public void Dispose() { Stop(); watcher.Received -= Advertisement; watcher.Stopped -= Stopped; }
}
