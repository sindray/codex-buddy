using System.Text;
using System.Text.Json;

namespace CodexBuddy.Core;

public sealed record BuddyPacket(string State = "idle", int Active = 0, int Elapsed = 0, string Task = "Codex task")
{
    public byte[] Encode(int seq) => Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new
    {
        v = 1, state = State, active = Math.Clamp(Active, 0, 99), elapsed = Math.Clamp(Elapsed, 0, 604800),
        task = Task, seq
        // Subtitles and theme synchronization are outside the Windows MVP scope.
    }) + "\n");
}

public sealed class ReplyStream
{
    readonly List<byte> buffer = [];
    bool overflow;
    public List<JsonElement> Feed(ReadOnlySpan<byte> bytes)
    {
        List<JsonElement> result = [];
        foreach (var b in bytes)
        {
            if (b == 10)
            {
                if (!overflow)
                {
                    try { using var doc = JsonDocument.Parse(buffer.ToArray()); if (doc.RootElement.ValueKind == JsonValueKind.Object) result.Add(doc.RootElement.Clone()); }
                    catch (JsonException) { }
                }
                buffer.Clear(); overflow = false;
            }
            else if (b != 13)
            {
                if (b == 0 || buffer.Count >= 4096) overflow = true;
                else if (!overflow) buffer.Add(b);
            }
        }
        return result;
    }
}

public sealed class Receipts
{
    readonly HashSet<int> pending = [];
    public double LastAccepted { get; private set; }
    public double LastResponse { get; private set; }
    public bool Busy { get; private set; }
    public void Sent(int seq) { pending.Add(seq); if (pending.Count > 32) pending.Remove(pending.Min()); }
    public bool Receive(JsonElement item, double now)
    {
        bool accepted = item.TryGetProperty("buddy_ack", out var raw);
        if (!accepted && !item.TryGetProperty("buddy_busy", out raw)) return false;
        if (raw.ValueKind != JsonValueKind.Number || !raw.TryGetInt32(out int seq) || !pending.Remove(seq)) return false;
        Busy = !accepted; LastResponse = now; if (accepted) LastAccepted = now;
        return true;
    }
    public bool Confirmed(double now) => !Busy && LastAccepted > 0 && now - LastAccepted < 6;
}
