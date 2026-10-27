using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Globalization;

namespace CodexBuddy.Core;

public sealed class RolloutSession(string path)
{
    public string Path { get; } = path;
    long offset;
    DateTime creation;
    readonly List<byte> buffer = [];
    bool oversized;
    public string Project { get; private set; } = "Codex task";
    public string State { get; private set; } = "idle";
    public bool Active { get; private set; }
    public bool Ignored { get; private set; }
    public double Started { get; private set; }
    public double Updated { get; private set; }
    string turn = "";
    readonly List<(string Id, string State)> pending = [];
    static string Text(JsonElement value, string name) => value.TryGetProperty(name, out var item) && item.ValueKind == JsonValueKind.String ? item.GetString()! : "";
    public static string Classify(string name, string raw)
    {
        name = name.ToLowerInvariant();
        if (name.Contains("request_user_input") || raw.Contains("request_user_input")) return "waiting";
        if (new[] { "apply_patch", "write_file", "edit_file" }.Any(name.Contains) || raw.Contains("tools.apply_patch(")) return "coding";
        if (new[] { "search", "web", "browse" }.Any(name.Contains) || raw.Contains("tools.web__run(")) return "searching";
        if (name.Contains("update_plan")) return "planning";
        (string Pattern, string State)[] rules = [
            (@"\b(?:pytest|ctest|unittest|vitest)\b|(?:npm|pnpm)(?:\.cmd)? (?:run )?test\b|\b(?:dotnet|cargo|go) test\b", "testing"),
            (@"\bidf\.py\b[^\n]{0,80}\b(?:build|reconfigure|merge-bin)\b|(?:npm|pnpm)(?:\.cmd)? run build|\b(?:dotnet|cargo|go) build\b|\bmsbuild\b|validate\.sh", "building"),
            (@"\bgit (?:diff|status)\b", "reviewing"), (@"\b(?:rg|cat|sed|head|tail|ls|find|Get-Content|Get-ChildItem)\b", "reading")];
        return rules.FirstOrDefault(r => Regex.IsMatch(raw, r.Pattern, RegexOptions.IgnoreCase)).State ?? "coding";
    }
    public void Consume(JsonElement value, double now)
    {
        if (value.ValueKind != JsonValueKind.Object || !value.TryGetProperty("payload", out var p) || p.ValueKind != JsonValueKind.Object) return;
        double stamp = DateTimeOffset.TryParse(Text(value, "timestamp"), CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var date) ? date.ToUnixTimeMilliseconds() / 1000d : now;
        string kind = Text(value, "type"), type = Text(p, "type");
        if (kind == "session_meta")
        {
            Ignored = p.TryGetProperty("source", out var source) && source.ValueKind == JsonValueKind.Object && source.TryGetProperty("subagent", out _);
            string name = Text(p, "cwd").Replace('\\', '/').TrimEnd('/').Split('/').Last();
            Project = name.Length is > 0 and <= 32 && name.All(c => c is >= ' ' and <= '~') ? name : "Codex task";
            return;
        }
        if (Ignored) return;
        if (kind == "event_msg")
        {
            if (type == "task_started") { Active = true; Started = Updated = stamp; turn = Text(p, "turn_id"); State = "received"; pending.Clear(); }
            else if (type is "task_complete" or "turn_aborted")
            {
                string finished = Text(p, "turn_id");
                if (finished.Length > 0 && turn.Length > 0 && finished != turn) return;
                bool error = p.TryGetProperty("error", out var e) && e.ValueKind is not (JsonValueKind.Null or JsonValueKind.False) && !(e.ValueKind == JsonValueKind.String && e.GetString() == "");
                Active = false; State = type == "turn_aborted" ? "cancelled" : error ? "error" : "done"; Updated = stamp; pending.Clear();
            }
        }
        else if (kind == "response_item" && Active)
        {
            string id = Text(p, "call_id");
            if (type is "function_call" or "custom_tool_call")
            {
                State = Classify(Text(p, "name"), Text(p, "arguments") is { Length: > 0 } a ? a : Text(p, "input"));
                pending.RemoveAll(x => x.Id == id); pending.Add((id, State)); Updated = stamp;
            }
            else if (type is "function_call_output" or "custom_tool_call_output") { pending.RemoveAll(x => x.Id == id); State = pending.LastOrDefault().State ?? "thinking"; Updated = stamp; }
            else if (type == "reasoning") { State = pending.LastOrDefault().State ?? "thinking"; Updated = stamp; }
        }
    }
    public void Read(double now)
    {
        try
        {
            var info = new FileInfo(Path);
            if (info.CreationTimeUtc != creation || info.Length < offset)
            {
                offset = 0; creation = info.CreationTimeUtc; buffer.Clear(); oversized = false;
                Project = "Codex task"; State = "idle"; Active = Ignored = false; Started = Updated = 0; pending.Clear(); turn = "";
            }
            using var file = new FileStream(Path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            file.Seek(offset, SeekOrigin.Begin);
            var bytes = new byte[64 * 1024]; int budget = 4 * 1024 * 1024, count;
            while (budget > 0 && (count = file.Read(bytes, 0, Math.Min(bytes.Length, budget))) > 0)
            {
                offset += count; budget -= count;
                foreach (var b in bytes.AsSpan(0, count))
                {
                    if (b == 10)
                    {
                        if (!oversized) { try { using var doc = JsonDocument.Parse(buffer.ToArray()); Consume(doc.RootElement, now); } catch (JsonException) { } }
                        buffer.Clear(); oversized = false;
                    }
                    else if (buffer.Count >= 8 * 1024 * 1024) { oversized = true; buffer.Clear(); }
                    else if (!oversized) buffer.Add(b);
                }
            }
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}

public sealed class StateMonitor(string root, double launched)
{
    public string Root { get; } = root;
    public string Health { get; private set; } = "正在检测 Codex 目录";
    readonly Dictionary<string, RolloutSession> sessions = [];
    double lastScan;
    public static string Resolve(string? saved = null)
    {
        string? env = Environment.GetEnvironmentVariable("CODEX_HOME");
        string folder = !string.IsNullOrWhiteSpace(saved) ? saved : !string.IsNullOrWhiteSpace(env) && System.IO.Path.IsPathFullyQualified(env) ? env : System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".codex");
        return System.IO.Path.GetFileName(folder.TrimEnd('\\', '/')) == "sessions" ? folder : System.IO.Path.Combine(folder, "sessions");
    }
    public BuddyPacket Poll(double now)
    {
        if (now - lastScan >= 5)
        {
            lastScan = now;
            if (!Directory.Exists(Root)) { sessions.Clear(); Health = "未找到 Codex 记录，请运行 Codex 或选择目录"; return new(); }
            try
            {
                _ = Directory.GetFileSystemEntries(Root);
                HashSet<string> recent = [];
                for (int day = 0; day <= 1; day++)
                {
                    string folder = System.IO.Path.Combine(Root, DateTimeOffset.FromUnixTimeSeconds((long)now).UtcDateTime.AddDays(-day).ToString("yyyy/MM/dd", CultureInfo.InvariantCulture));
                    if (Directory.Exists(folder)) foreach (string path in Directory.GetFiles(folder, "*.jsonl")) recent.Add(path);
                }
                foreach (string old in sessions.Keys.Except(recent).ToArray()) sessions.Remove(old);
                foreach (string path in recent) sessions.TryAdd(path, new(path));
                Health = recent.Count > 0 ? "已发现近期 Codex 记录" : "近两天暂无记录，请在 Codex 发起任务";
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException) { sessions.Clear(); Health = "无法读取 Codex 目录，请重新选择"; return new(); }
        }
        foreach (var session in sessions.Values) session.Read(now);
        var items = sessions.Values.Where(s => !s.Ignored).ToArray();
        var active = items.Where(s => s.Active && now - s.Updated < 3600).ToArray();
        int Priority(string state) => state == "approval" ? 6 : state == "waiting" ? 5 : state == "error" ? 4 : 0;
        var selected = active.OrderByDescending(s => Priority(s.State)).ThenByDescending(s => s.Updated).FirstOrDefault();
        if (active.Length == 0)
        {
            selected = items.Where(s => !s.Active && s.Updated >= launched).OrderByDescending(s => s.Updated).FirstOrDefault();
            if (selected != null && selected.State != "error" && now - selected.Updated >= 12) selected = null;
        }
        string state = selected?.State ?? "idle";
        if (state == "received" && selected != null && now - selected.Started > 3) state = "thinking";
        return new(state, Math.Min(active.Length, 99), active.Length == 0 ? 0 : (int)Math.Clamp(now - (selected?.Started ?? now), 0, 604800), selected?.Project ?? "Codex task");
    }
}
