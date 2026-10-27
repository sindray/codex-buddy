using System.Text;
using System.Text.Json;
using CodexBuddy.Core;

int checks = 0;
void Check(bool condition, string message) { checks++; if (!condition) throw new Exception(message); }
JsonElement Json(string text) { using var doc = JsonDocument.Parse(text); return doc.RootElement.Clone(); }
void Event(RolloutSession session, string kind, object payload, double now = 100) => session.Consume(Json(JsonSerializer.Serialize(new { type = kind, payload })), now);
var packet = new BuddyPacket("coding", 999, -1, "project");
using (var doc = JsonDocument.Parse(packet.Encode(42)))
{
    var p = doc.RootElement;
    Check(p.GetProperty("seq").GetInt32() == 42 && p.GetProperty("active").GetInt32() == 99 && p.GetProperty("elapsed").GetInt32() == 0, "Protocol fields/clamping");
    Check(!p.TryGetProperty("caption", out _), "No subtitles in Windows packet");
}
var stream = new ReplyStream();
Check(stream.Feed(Encoding.UTF8.GetBytes("{\"buddy_ack\":")).Count == 0, "Fragment held");
var replies = stream.Feed(Encoding.UTF8.GetBytes("42}\r\ninvalid\n"));
Check(replies.Count == 1, "Fragment framing and invalid JSON recovery");
var receipts = new Receipts();
Check(!receipts.Receive(replies[0], 100), "Reject unsolicited ack");
receipts.Sent(42); Check(receipts.Receive(replies[0], 100) && receipts.Confirmed(105) && !receipts.Confirmed(106), "Matched ack and expiry");
Check(!receipts.Receive(replies[0], 101), "Reject duplicate");
receipts.Sent(45); receipts.Receive(Json("{\"buddy_busy\":45}"), 101); Check(!receipts.Confirmed(101), "Busy immediately overrides a recent ack");
receipts.Sent(43); Check(receipts.Receive(Json("{\"buddy_busy\":43}"), 107) && receipts.Busy && !receipts.Confirmed(107), "USB busy is not BLE success");
receipts.Sent(44); Check(!receipts.Receive(Json("{\"buddy_ack\":true}"), 108), "Reject boolean ack");
var oversized = new byte[5000]; Array.Fill(oversized, (byte)'x'); stream.Feed(oversized);
Check(stream.Feed(Encoding.UTF8.GetBytes("\n{\"buddy_ack\":44}\n")).Count == 1, "Oversize frame recovery");
var session = new RolloutSession("unused");
Event(session, "session_meta", new { cwd = @"C:\work\buddy" }); Check(session.Project == "buddy", "Windows project path");
Event(session, "event_msg", new { type = "task_started", turn_id = "new" });
Event(session, "response_item", new { type = "function_call", call_id = "a", name = "request_user_input", arguments = "{}" });
Event(session, "response_item", new { type = "function_call", call_id = "b", name = "exec_command", arguments = "npm test" });
Check(session.State == "testing", "Tool classification");
Event(session, "response_item", new { type = "function_call_output", call_id = "b" }); Check(session.State == "waiting", "Pending parallel tool preserved");
Event(session, "event_msg", new { type = "task_complete", turn_id = "old" }); Check(session.Active, "Old completion cannot end new turn");
Event(session, "response_item", new { type = "reasoning" }); Check(session.State == "waiting", "Reasoning does not clear pending input");
Event(session, "event_msg", new { type = "task_complete", turn_id = "new" }); Check(!session.Active && session.State == "done", "Completion");
var subagent = new RolloutSession("unused"); Event(subagent, "session_meta", new { cwd = "/repo", source = new { subagent = "test" } }); Event(subagent, "event_msg", new { type = "task_started" }); Check(subagent.Ignored && !subagent.Active, "Subagent excluded");
Check(RolloutSession.Classify("exec_command", "git diff") == "reviewing" && RolloutSession.Classify("exec_command", "pnpm run build") == "building", "Command states");
Check(RolloutSession.Classify("exec_command", "dotnet test") == "testing" && RolloutSession.Classify("exec_command", "Get-Content README.md") == "reading", "Windows command states");
string root = Path.Combine(Path.GetTempPath(), "buddy-tests-" + Guid.NewGuid());
try
{
    double now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
    string folder = Path.Combine(root, DateTime.UtcNow.ToString("yyyy/MM/dd")); Directory.CreateDirectory(folder);
    string file = Path.Combine(folder, "rollout-test.jsonl");
    string started = JsonSerializer.Serialize(new { type = "event_msg", timestamp = DateTimeOffset.UtcNow.ToString("O"), payload = new { type = "task_started", turn_id = "a" } });
    File.WriteAllText(file, started[..20]); var monitor = new StateMonitor(root, now - 1);
    Check(monitor.Poll(now).Active == 0, "Partial line not consumed");
    File.AppendAllText(file, started[20..] + "\n"); Check(monitor.Poll(now + 1).Active == 1, "Appended line consumed without rescan");
    Check(monitor.Poll(now + 4).State == "thinking", "Received transitions to thinking");
    File.WriteAllText(file, "{}\n"); Check(monitor.Poll(now + 5).Active == 0, "Truncation resets active task");
    string EventLine(object payload, double at, string kind = "event_msg") => JsonSerializer.Serialize(new { type = kind, timestamp = DateTimeOffset.FromUnixTimeMilliseconds((long)(at * 1000)).ToString("O"), payload }) + "\n";
    string waiting = Path.Combine(folder, "waiting.jsonl"), working = Path.Combine(folder, "working.jsonl");
    File.WriteAllText(waiting, EventLine(new { type = "task_started", turn_id = "wait" }, now + 5) + EventLine(new { type = "function_call", call_id = "wait", name = "request_user_input", arguments = "{}" }, now + 6, "response_item"));
    File.WriteAllText(working, EventLine(new { type = "task_started", turn_id = "work" }, now + 7));
    var parallel = new StateMonitor(root, now);
    Check(parallel.Poll(now + 8) is { State: "waiting", Active: 2 }, "Waiting task outranks a newer working task");
    File.AppendAllText(waiting, EventLine(new { type = "task_complete", turn_id = "wait" }, now + 9));
    File.AppendAllText(working, EventLine(new { type = "task_complete", turn_id = "work" }, now + 9));
    Check(parallel.Poll(now + 10) is { State: "done", Active: 0 }, "Multiple sessions complete");
    Check(parallel.Poll(now + 22).State == "idle", "Completion expires to idle");
    var missing = new StateMonitor(Path.Combine(root, "missing"), now); Check(missing.Poll(now).State == "idle" && missing.Health.Contains("未找到"), "Missing root diagnostic");
    Check(StateMonitor.Resolve(Path.Combine(root, "sessions")) == Path.Combine(root, "sessions"), "Sessions directory accepted directly");
}
finally { Directory.Delete(root, true); }
Console.WriteLine($"PASS: {checks} checks (protocol, receipts, task parser, incremental files)");
