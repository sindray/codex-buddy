using CodexBuddy.Core;
using System.Text.Json;

namespace CodexBuddy.Windows;

static class Program
{
    [STAThread]
    static void Main()
    {
        using var single = new Mutex(true, @"Local\CodexBuddy.Windows", out bool created);
        if (!created) { MessageBox.Show("Codex Buddy 已在任务栏托盘运行。", "Codex Buddy"); return; }
        ApplicationConfiguration.Initialize();
        Application.Run(new BuddyContext());
    }
}

sealed class Preferences
{
    public string Mode { get; set; } = "auto";
    public string? Folder { get; set; }
    public string? Usb { get; set; }
    public string? Ble { get; set; }
    public static readonly string DirectoryPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CodexBuddy");
    static string FilePath => Path.Combine(DirectoryPath, "settings.json");
    public static Preferences Load()
    {
        try { var p = JsonSerializer.Deserialize<Preferences>(File.ReadAllText(FilePath)) ?? new(); if (p.Mode is not ("auto" or "usb" or "ble")) p.Mode = "auto"; return p; }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException) { return new(); }
    }
    public void Save() { Directory.CreateDirectory(DirectoryPath); string temp = FilePath + ".tmp"; File.WriteAllText(temp, JsonSerializer.Serialize(this)); File.Move(temp, FilePath, true); }
}

sealed class BuddyContext : ApplicationContext
{
    readonly Control dispatcher = new();
    readonly NotifyIcon tray;
    readonly System.Windows.Forms.Timer timer = new() { Interval = 1000 };
    readonly ContextMenuStrip menu = new();
    readonly Preferences prefs = Preferences.Load();
    readonly UsbLink usb;
    readonly BleLink ble;
    StateMonitor monitor;
    Receipts usbReceipt = new(), bleReceipt = new();
    DeviceChoice[] usbDevices = [];
    BuddyPacket packet = new();
    bool paused, ticking, exiting;
    int sequence = Random.Shared.Next(10000, 1000000), epoch;
    double lastDiscovery, usbAttempt, bleAttempt, usbRetry, bleRetry;
    string usbStatus = "等待 USB 设备", bleStatus = "等待蓝牙设备";
    string Status => paused ? "已暂停同步" : usb.Open && usbReceipt.Confirmed(Now) ? "USB 已连接 · 正在同步" : ble.Ready && bleReceipt.Confirmed(Now) ? "蓝牙已连接 · 正在同步" : bleReceipt.Busy ? "设备正由另一条 USB 连接同步" : prefs.Mode == "ble" || prefs.Mode == "auto" && !usb.Open && usbDevices.Length == 0 ? bleStatus : usbStatus;
    static double Now => DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() / 1000d;
    public BuddyContext()
    {
        _ = dispatcher.Handle;
        void Dispatch(Action action) { if (!exiting && !dispatcher.IsDisposed) { try { dispatcher.BeginInvoke(action); } catch (InvalidOperationException) { } } }
        usb = new(Dispatch); ble = new(Dispatch);
        usb.Reply = value => usbReceipt.Receive(value, Now); ble.Reply = value => bleReceipt.Receive(value, Now);
        monitor = new(StateMonitor.Resolve(prefs.Folder), Now);
        tray = new() { Icon = MakeIcon(), Text = "Codex Buddy", ContextMenuStrip = menu, Visible = true };
        menu.Opening += (_, _) => BuildMenu(); tray.DoubleClick += (_, _) => ShowGuide();
        timer.Tick += async (_, _) => await Tick(); timer.Start(); BuildMenu();
        if (!File.Exists(Path.Combine(Preferences.DirectoryPath, "settings.json"))) { Save(); ShowGuide(); }
    }
    static Icon MakeIcon()
    {
        using var bitmap = new Bitmap(32, 32); using var g = Graphics.FromImage(bitmap); g.Clear(Color.FromArgb(12, 19, 25));
        using var brush = new SolidBrush(Color.FromArgb(91, 216, 230)); g.FillEllipse(brush, 6, 8, 8, 16); g.FillEllipse(brush, 19, 8, 8, 16);
        var handle = bitmap.GetHicon(); try { using var borrowed = Icon.FromHandle(handle); return (Icon)borrowed.Clone(); } finally { DestroyIcon(handle); }
    }
    [System.Runtime.InteropServices.DllImport("user32.dll")] [return: System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.Bool)] static extern bool DestroyIcon(IntPtr icon);
    ToolStripMenuItem Item(string title, Action click, bool check = false) { var item = new ToolStripMenuItem(title) { Checked = check }; item.Click += (_, _) => click(); return item; }
    static string StateTitle(string state) => state switch
    {
        "received" => "收到任务", "thinking" => "思考中", "planning" => "规划中", "reading" => "阅读中",
        "searching" => "搜索中", "coding" => "编写中", "building" => "构建中", "testing" => "测试中",
        "reviewing" => "检查中", "waiting" => "等待回答", "approval" => "等待审批", "done" => "已完成",
        "error" => "遇到问题", "cancelled" => "已取消", _ => "待命"
    };
    void Save() { try { prefs.Save(); } catch (Exception e) when (e is IOException or UnauthorizedAccessException) { MessageBox.Show("无法保存设置，请检查当前用户目录权限。", "Codex Buddy"); } }
    void BuildMenu()
    {
        foreach (ToolStripItem old in menu.Items.Cast<ToolStripItem>().ToArray()) old.Dispose();
        menu.Items.Clear(); menu.Items.Add(new ToolStripMenuItem("Codex Buddy · " + Status) { Enabled = false });
        menu.Items.Add(new ToolStripMenuItem("任务 · " + StateTitle(packet.State) + " · " + packet.Active + " 个进行中") { Enabled = false });
        menu.Items.Add(new ToolStripMenuItem(monitor.Health) { Enabled = false }); menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add(Item(paused ? "恢复同步" : "暂停同步", () => { paused = !paused; Reconnect(); }));
        var settings = new ToolStripMenuItem("设置");
        var mode = new ToolStripMenuItem("连接方式");
        foreach (var (name, value) in new[] { ("自动 · USB 优先", "auto"), ("仅 USB", "usb"), ("仅蓝牙", "ble") }) mode.DropDownItems.Add(Item(name, () => { prefs.Mode = value; Save(); Reconnect(); }, prefs.Mode == value));
        settings.DropDownItems.Add(mode);
        var usbMenu = new ToolStripMenuItem("USB 设备"); usbMenu.DropDownItems.Add(Item("自动选择单个设备", () => { prefs.Usb = null; Save(); Reconnect(); }, prefs.Usb == null));
        foreach (var d in usbDevices) usbMenu.DropDownItems.Add(Item(d.Name, () => { prefs.Usb = d.Id; Save(); Reconnect(); }, prefs.Usb == d.Id));
        if (usbDevices.Length == 0) usbMenu.DropDownItems.Add(new ToolStripMenuItem("未发现 AI Passport") { Enabled = false }); settings.DropDownItems.Add(usbMenu);
        var bleMenu = new ToolStripMenuItem("蓝牙设备"); bleMenu.DropDownItems.Add(Item("自动选择单个设备", () => { prefs.Ble = null; Save(); Reconnect(); }, prefs.Ble == null));
        foreach (var d in ble.Devices(Now)) bleMenu.DropDownItems.Add(Item(d.Name, () => { prefs.Ble = d.Id; Save(); Reconnect(); }, prefs.Ble == d.Id));
        if (ble.Devices(Now).Length == 0) bleMenu.DropDownItems.Add(new ToolStripMenuItem(ble.ScanStatus) { Enabled = false }); settings.DropDownItems.Add(bleMenu);
        settings.DropDownItems.Add(Item("重新连接", Reconnect));
        settings.DropDownItems.Add(Item("选择 Codex 目录…", () => { using var dialog = new FolderBrowserDialog { Description = "选择 Codex 数据目录或 sessions 文件夹" }; if (dialog.ShowDialog() == DialogResult.OK) { prefs.Folder = dialog.SelectedPath; Save(); monitor = new(StateMonitor.Resolve(prefs.Folder), Now); } }));
        settings.DropDownItems.Add(Item("恢复目录自动检测", () => { prefs.Folder = null; Save(); monitor = new(StateMonitor.Resolve(), Now); }));
        menu.Items.Add(settings); menu.Items.Add(Item("使用说明…", ShowGuide)); menu.Items.Add(new ToolStripSeparator()); menu.Items.Add(Item("退出 Codex Buddy", ExitThread));
    }
    void ShowGuide() => MessageBox.Show("1. 打开本机 Codex，给 AI Passport 通电。\n2. 自动模式优先 USB；蓝牙无需系统配对，直接在设置中选择设备。\n3. 未检测到任务时，选择 Codex 数据目录或 sessions 文件夹。\n\n程序在托盘后台运行，默认不随系统启动。蓝牙需电脑支持 BLE 并开启系统蓝牙；仅 USB 无需蓝牙。此版本不读取输入文字、不提供字幕，也不需要辅助功能、输入监控或管理员权限。\n\n仅在本机解析最近两天 Codex 记录，向设备发送任务状态、任务数、用时和项目名，不上传云端。\n\n版本 0.1.0 · 需要兼容 Codex Buddy v1 的设备固件。", "Codex Buddy · Windows");
    void Reconnect() { epoch++; usb.Dispose(); ble.Stop(); usbReceipt = new(); bleReceipt = new(); usbRetry = bleRetry = lastDiscovery = 0; }
    async Task Tick()
    {
        if (ticking || exiting) return; ticking = true; int current = epoch;
        try
        {
            double now = Now; var source = monitor; var latest = await Task.Run(() => source.Poll(now));
            if (source != monitor) return; packet = latest;
            if (current != epoch || exiting || paused) return;
            if (now - lastDiscovery >= 5 && prefs.Mode != "ble")
            {
                try { usbDevices = await Task.Run(UsbLink.Discover); }
                catch (Exception e) when (e is System.Management.ManagementException or UnauthorizedAccessException or System.Runtime.InteropServices.COMException) { usbDevices = []; usbStatus = "无法枚举 USB，请检查系统设备管理器"; }
                if (current != epoch || exiting) return; lastDiscovery = now;
            }
            if (prefs.Mode != "ble")
            {
                if (usb.Id != null && !usbDevices.Any(d => d.Id == usb.Id)) { usb.Dispose(); usbReceipt = new(); }
                if (!usb.Open && now >= usbRetry)
                {
                    var candidates = usbDevices.Where(d => prefs.Usb == null || prefs.Usb == d.Id).ToArray();
                    if (candidates.Length == 1)
                    {
                        try { usb.Connect(candidates[0].Id); usbAttempt = now; usbReceipt = new(); usbStatus = "USB 已打开，等待设备确认"; }
                        catch (Exception e) when (e is IOException or UnauthorizedAccessException or InvalidOperationException) { usbStatus = "USB 无法打开，可能被其他程序占用"; }
                    }
                    else usbStatus = candidates.Length > 1 ? "发现多个 USB 设备，请在设置中选择" : "等待 USB 设备";
                    usbRetry = now + 3;
                }
                if (usb.Open)
                {
                    int seq = Next(); usbReceipt.Sent(seq);
                    try { await usb.Send(packet.Encode(seq)); }
                    catch (Exception e) when (e is IOException or InvalidOperationException or UnauthorizedAccessException or TimeoutException) { usb.Dispose(); usbReceipt = new(); usbStatus = "USB 已断开，正在重连"; usbRetry = now + 3; }
                    if (current != epoch || exiting) return;
                    if (now - Math.Max(usbAttempt, usbReceipt.LastAccepted) > 10) { usb.Dispose(); usbReceipt = new(); usbStatus = "USB 未收到回执，正在重试"; usbRetry = now + 5; }
                }
            }
            if (prefs.Mode == "usb" || usb.Open && (usbReceipt.Confirmed(now) || now - usbAttempt < 3)) { ble.Stop(); bleReceipt = new(); }
            else
            {
                ble.StartScan();
                if (!ble.Ready && now >= bleRetry)
                {
                    var candidates = ble.Devices(now).Where(d => prefs.Ble == null || prefs.Ble == d.Id).ToArray();
                    if (candidates.Length == 1)
                    {
                        bleStatus = "正在连接蓝牙…"; tray.Text = "Codex Buddy · " + bleStatus;
                        try { await ble.Connect(candidates[0].Id); bleAttempt = Now; bleReceipt = new(); bleStatus = "蓝牙已连接，等待设备确认"; }
                        catch (OperationCanceledException) { return; }
                        catch (Exception e) when (e is not OutOfMemoryException) { bleStatus = "蓝牙连接失败，请检查设备、蓝牙及权限"; }
                    }
                    else bleStatus = candidates.Length > 1 ? "发现多个蓝牙设备，请在设置中选择" : ble.ScanStatus;
                    bleRetry = Now + 5;
                    if (current != epoch || exiting) return;
                }
                if (ble.Ready)
                {
                    int seq = Next(); bleReceipt.Sent(seq);
                    try { await ble.Send(packet.Encode(seq)); }
                    catch (OperationCanceledException) { return; }
                    catch (Exception e) when (e is not OutOfMemoryException) { ble.Disconnect(); bleReceipt = new(); bleStatus = "蓝牙已断开，正在重连"; bleRetry = now + 3; }
                    if (current != epoch || exiting) return;
                    if (now - Math.Max(bleAttempt, bleReceipt.LastResponse) > 10) { ble.Disconnect(); bleReceipt = new(); bleRetry = now + 3; bleStatus = "蓝牙未收到回执，正在重连"; }
                }
            }
            tray.Text = ("Codex Buddy · " + Status)[..Math.Min(63, ("Codex Buddy · " + Status).Length)];
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Runtime.InteropServices.COMException or System.Management.ManagementException)
        { usbStatus = "连接暂时不可用，将自动重试"; bleStatus = usbStatus; }
        finally { ticking = false; }
    }
    int Next() { sequence = sequence >= int.MaxValue - 1 ? 1 : sequence + 1; return sequence; }
    protected override void ExitThreadCore()
    {
        exiting = true; epoch++; timer.Stop(); timer.Dispose(); usb.Dispose(); ble.Dispose(); tray.Visible = false; tray.Icon?.Dispose(); tray.Dispose(); menu.Dispose(); dispatcher.Dispose(); base.ExitThreadCore();
    }
}
