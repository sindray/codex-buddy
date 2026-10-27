[简体中文](CODEX_BUDDY.zh_CN.md)

# Codex Buddy 0.4.8

Custom firmware for the FoloToy AI Passport ESP32-C3 (8 MB flash, 240 × 320
ST7789P3). Uses the official BSP and replaces the factory demo entry point.
The current design uses a graphite face enclosure, large-radius cyan eyes and
separate battery/task telemetry. Bluetooth LE is optional; no microphone, Wi-Fi or cloud API is used.

## 0.4.8 — Optional live input captions

Default volume is now 10% at boot and when restoring sound from zero. Manual volume adjustment remains available.

Desktop 0.4.0 can send an optional `caption` containing exactly 432 bytes encoded as 576 Base64 characters: a 192×18, row-major, MSB-first monochrome raster. The newline-delimited JSON limit is now 1023 bytes, allowing the caption and existing v1 metadata together. USB and BLE retain the same state/receipt protocol and priority policy. Old senders omit the field and behave as before; malformed or absent captions clear the previous image. Captions are memory-only and are never logged.

The bottom line displays the latest input tail while the eyes, task title and battery remain visible. Captions expire five seconds after their image changes, fading over the final 500 ms; gallery, detail and volume views hide them. A lost desktop connection cannot leave text visible indefinitely. Arbitrary Chinese text is rasterized by the Mac instead of adding a full firmware font. Desktop collection is opt-in and requires macOS Accessibility permission; see the paired desktop README for controls and limitations.

Desktop 0.4.1 improves caption raster clarity and coalesces edits without changing the wire format. Build: PASS (ESP-IDF 5.5.3). Host tests: PASS (firmware complete gate and desktop protocol/raster/edit-coalescing tests). Device tests: PASS for segmented flashing with image verification, version 0.4.8 boot with PCM volume 10%, USB Chinese-caption/state/clear receipts, audio-worker cue completion, and desktop 0.4.1 reconnect with a fresh USB-confirmed diagnostic. Physical rendering and perceived audio were not observed. Unverified: physical text clarity, real editor/IME behavior, USB/BLE live input and perceived loudness. Verified archive: `build/firmware/add5d37622fb90950caa62094e20f63e15da045220834ea4755edd8445720763/`; full-image SHA-256 matches the directory name, ELF SHA-256: `2a7f33ed0c8fa12db7bce8d8cd359dba3fc54b31be8874d63e944004a40a7c21`.

## 0.4.7 — USB and Bluetooth LE

The same firmware accepts newline-delimited v1 JSON over USB or BLE (NimBLE,
one peripheral connection). Existing expressions, buttons and 19 sound cues
are retained. The status line indicates USB/BLE. Valid USB packets take priority
for five seconds after their last arrival; BLE can take over after that lease.
Each transport has its own bounded framer and latest-packet queue. Oversized or
NUL-containing lines are dropped through the next newline; disconnects discard
partial BLE frames. BLE replies are queued onto the host task and tied to a
connection generation, so stale replies cannot reach a reconnected client.

BLE advertises `CodexBuddy`, service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`.
RX is `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (write); TX is
`6e400003-b5a3-f393-e0a9-e50e24dcca9e` (notify). Both directions support
20-byte fragments. Replies contain `buddy_ack`; while USB owns the lease,
BLE receives `buddy_busy` instead. The bridge reconnects and requires matching
sequence acknowledgements. It sends only existing task metadata, never prompts
or answers. This local status service is unpaired and does not provide remote
command execution or audio streaming. Radio initialization failure retains USB;
NVS is never erased to recover initialization.

USB invocation and dependency requirements are unchanged. Optional BLE setup:

```sh
python3 -m venv runtime/ble-venv
runtime/ble-venv/bin/python -m pip install -r tools/buddy/requirements-ble.txt
runtime/ble-venv/bin/python tools/buddy/bridge.py --transport ble
# Multiple devices: add --ble-address <address-or-macOS-peripheral-UUID>
```

Allow Bluetooth access for the launching terminal/application in macOS settings.
The bridge discovers by service UUID. Only one local BLE bridge runs at a time;
USB may continue running concurrently, retaining priority until stopped. USB
power alone does not take priority: only valid state packets do. The device
still needs power during wireless use. The optional client uses Bleak 0.22.3
for compatibility with Python 3.9; USB does not import that dependency.

Build, full host gate and archive verification: PASS (ESP-IDF v5.5.4; repository baseline v5.5.3).
BLE dependencies installed in `runtime/ble-venv`; import and metadata dry-run pass.
Device smoke tests: PASS for approved 0.4.7, flashed to USB device 98:C3:77:F4:29:4C.
Verified startup identity/fonts/audio completion, USB heartbeat/disconnect/reconnect,
BLE discovery, connection, fragmented writes/notifications, USB priority, BLE
takeover after the lease, and BLE disconnect/reconnect. The actual BLE bridge
also acknowledged live Codex metadata after USB stopped. Both local bridges
are running; USB is preferred while its heartbeat continues. No system service
was installed. Startup free heap: 105,916 bytes; largest block: 94,208 bytes.
Evidence stays local: `runtime/flash-0.4.7.log`, `runtime/hardware-smoke-0.4.7.log`,
`runtime/acceptance-0.4.7.json`, `runtime/ble-acceptance-0.4.7.json`,
`runtime/ble-bridge-0.4.7.log` and `runtime/bridge-0.4.7.log`.
Full image SHA-256: `36f200fe88b269531c9464af21e8c53b1c076f45acb306301415014ea6acfae4`.
Matching ELF SHA-256: `6859f247857f34802704a2b5308b9956c85e58e087446860b8b00d4d0d8a78aa`.
Verified archive: `build/firmware/36f200fe88b269531c9464af21e8c53b1c076f45acb306301415014ea6acfae4/`.
Unverified: visual smoothness under radio load, audible sound/volume and physical
keys, RF range, battery endurance and long-duration stability.

## 0.4.6 — Restore a useful loudness range

The 0.4.5 volume control changed PCM amplitude, but its codec reference of 50
still imposed about -25 dB under the default codec curve, on top of low synth
peaks. This was an overly conservative output ceiling. Codec reference is now
90 (about -5 dB), and each synth voice's peak ceiling is four times higher,
with no change to pitch, envelope or relative timbre. The nominal digital/codec
increase is about 32 dB at equal UI settings; this is not a measured acoustic
increase. Software gain remains 0–100%, smoothly ramped, with zero/mute silent.
Default volume is 50% after boot and when unmuting from zero.

Host tests check every cue's signed PCM bounds and chunk joins, plus volume
energy, mute and ramps. No analog clipping or speaker loudness is established
by these checks. All 19 cues measured peaks of 9,593–14,929 at 100% software gain (16-bit
full-scale magnitude 32,768). Build, host tests and archive verification: PASS
(ESP-IDF v5.5.4). Flashed to the approved device. Device smoke tests: PASS
for firmware identity, default 50%/codec-90 initialization, cue completions,
state acknowledgements and disconnect/reconnect. Bridge restored. Physical
loudness, distortion and volume-key behavior require user acceptance. Evidence:
`runtime/flash-0.4.6.log`, `runtime/hardware-smoke-0.4.6.log` and
`runtime/acceptance-0.4.6.json`.
Full image SHA-256: `d3b4416633656f764d2ea97a79597677a8b6af1767b0c09ac7aba2def3f51a96`.
Matching ELF SHA-256: `a61a0323cbc637134f9e0851674943c21641854265be46da41d7f41da7936a26`.
Verified archive: `build/firmware/d3b4416633656f764d2ea97a79597677a8b6af1767b0c09ac7aba2def3f51a96/`.

## 0.4.5 — PCM volume control

The user observed changing percentages without an audible level change. The
previous app changed only codec volume and had no PCM gain stage; a codec write
failure was not observable through the BSP's void setter. The exact hardware
cause is unconfirmed. The app now holds codec volume at a reference of 50 and
applies the displayed 0–100% directly to signed 16-bit PCM. Default 25% is about
12 dB below that reference, close to the previous default under the codec's
standard curve. Every 256-sample block ramps to its target over 16 ms; mute
ramps to zero during playback. Existing controls and the 96 ms drain remain.

Host regression tests check monotonic sample energy, full-scale signed bounds,
zero/mute, gain ramps and a real emotional cue at 25/50/100%. These do not prove
speaker loudness or button behavior. Build and host tests: PASS (ESP-IDF v5.5.4). Flashed to the approved device.
Device smoke tests: PASS for firmware identity, PCM-volume startup, seven cue
completions, state acknowledgements and disconnect/reconnect. Bridge restored.
Physical keys, audible volume differences and mute remain unverified by the
user. Evidence is local under `runtime/flash-0.4.5.log`,
`runtime/hardware-smoke-0.4.5.log` and `runtime/acceptance-0.4.5.json`. Full image SHA-256: `b4d668cfa9e0a0ba156cf86e4a9a10ca8d6f25e06d151d58f11929f92c309bc2`.
Matching ELF SHA-256: `d3657cf2237c5fa3b3f07800d4822962323ed75b9afcee17a49ded4ede45d07c`.
Verified archive: `build/firmware/b4d668cfa9e0a0ba156cf86e4a9a10ca8d6f25e06d151d58f11929f92c309bc2/`.

## 0.4.4 — Restrained expressions, transitions and richer sound

The gradient/color experiment was withdrawn before flashing. Eyes retain the
single cyan palette, dimmed for sleep. All diagonal background masks are removed;
lid gestures lift and compress complete rounded eyes. Thought includes a short
asymmetric lift, waiting includes an occasional upward glance, and receipt opens
the eyes briefly before settling. Happy blinks retain the curved eye.

State transitions ease over 12 target frames (about 0.4 seconds at 30 Hz), starting
from the displayed pose when interrupted. Greeting uses time since entering the
state for its wink, rather than an arbitrary point on a repeating global clock.
Global gaze and ordinary blink timing continue across state changes.

Five new original bit cues cover reading, review, building, testing and waking.
Soft task cues share the existing six-second cooldown and repeated state packets
remain silent. Wake, receipt, completion, errors, waiting/approval and greetings
remain responsive. Volume, buttons and task telemetry retain their behavior.
The sound engine now uses interpolated sine synthesis with two quiet harmonics,
per-emotion timbre/peak levels, continuous short pitch slides, an 8 ms rounded
attack and up to 25 ms release. Output remains 16 kHz / signed 16-bit mono;
this changes the timbre rather than claiming a bit-depth upgrade. No square-wave
oscillator, per-sample floating point, external assets or extra audio buffers
are required. Host checks cover non-clipping PCM, smooth sample joins and exact
chunk-independent rendering. Speaker quality and real-time CPU headroom remain
unverified on hardware.

Host tests cover geometry, arrival timing, transition interruption, sound mapping,
cooldown and PCM bounds. Physical appearance, smoothness and sound quality still
require user observation on the device. This revision has been flashed.

Build, host tests and archive verification: PASS (ESP-IDF v5.5.4).
Device tests: PASS for startup identity/font coverage, state acknowledgements,
all twelve emotional/task cue completions, disconnect/reconnect and bridge
restoration. Physical appearance, audible quality, buttons/volume and CPU
headroom remain unverified. Local evidence: `runtime/flash-0.4.4.log`,
`runtime/hardware-smoke-0.4.4.log`, `runtime/acceptance-0.4.4.json`.
Merged image SHA-256: `c0dd3c2d826c5d36d16dd9d83adf0d7d7e895712e83f9452075bafcd94507745`.
Matching ELF SHA-256: `9e4f59e18b9a4da20b07a70db041a9da6fc1d8e9e16688a7949d3481ede321c8`.
Archive: `build/firmware/c0dd3c2d826c5d36d16dd9d83adf0d7d7e895712e83f9452075bafcd94507745/`.

## 0.4.3 — Smile and thinking-eye correction

Happy eyes use a fuller 14-pixel stroke and a shallower nine-pixel arch with
12 segments per layer. Happy blinks narrow the same curved eye instead of
unfolding it through a capsule into an open eye. The frame, task layout and
emotional sounds are unchanged. Host checks cover smile geometry and a fully
closed happy blink. Physical appearance and smoothness require device testing.

Thinking no longer uses a dark diagonal cut across the small eye. Both eyes
retain complete rounded corners; the left height increases from 12 to 16 while
the right stays at 22, with widths 46/50. Host checks cover the idle-to-thinking
transition and require two intact eyes without a diagonal mask. This revision
has not been flashed.

Build, host tests and archive verification: PASS (ESP-IDF v5.5.4).
Device tests: NOT RUN. Merged image SHA-256: `c4fa62eee9aac03639a85bb7a601ebcc9949a272cca38c0d44d08cd8a1a13e17`.
Matching ELF SHA-256: `ebfa5afba5c6efb83623625e95d10cafe0af7522762198ce5c06383e803a3042`.
Archive: `build/firmware/c4fa62eee9aac03639a85bb7a601ebcc9949a272cca38c0d44d08cd8a1a13e17/`.

## 0.4.2 — More expressive eyes and emotional sounds

The same restrained face now uses distinct upper-lid slopes, eye widths and
asymmetry for curiosity, focus, anticipation and disappointment. Happy eyes
close into capsules and bend into rounded arches, instead of cutting semicircles
out of square eyes. Idle occasionally raises one eye or winks, and includes an
occasional double blink. Width, lid slope and curvature interpolate with task
transitions; the graphite enclosure and status layout stay fixed.

Seven new original bit-style cues cover questions, anticipation, focus, searching,
disappointment, sleep and lost connection. Cues follow visible state changes,
including the manual gallery and automatic sleep/disconnection. Related thinking
or work states do not retrigger the same cue. Automatic soft cues share a six-second
cooldown; task receipt, completion, errors, waiting/approval and user greetings
remain responsive. Idle gestures and duplicate heartbeats are silent. Volume and
mute controls remain unchanged; no official EMO recordings are used.

Host checks cover emotional cue mapping/cooldown, a curved-smile geometry contract,
distinct state silhouettes, idle gestures and 26,400 geometry frames. Build and
host results do not verify physical smoothness or speaker quality. Firmware and
matching ELF identity are retained in the content-addressed build archive.

Validated and flashed 0.4.2 artifact:
- Merged image SHA-256: `4a24e86046b8df2cf683ef41ec298618f28bda746aef4526d92192da04ddb5a4`.
- Matching ELF SHA-256: `317ed19c9443f9fb557d9f6665e227349de1584a404d1c985bb98d51aa5fbf3c`.
- Archive: `build/firmware/4a24e86046b8df2cf683ef41ec298618f28bda746aef4526d92192da04ddb5a4/`.
- Build and host tests: PASS. Flash write/hash verification: PASS. Device startup,
  matching version/ELF prefix, font coverage, emotional-state acknowledgements,
  all seven new cue playback completions, disconnect/reconnect and restored
  bridge acknowledgement: PASS. Logs are retained locally under `runtime/`.
- Physical rendering/smoothness, audible quality and key/volume behavior remain
  unverified; playback logs do not establish speaker quality.

## 0.4.1 — Basic expressive motion

Replaces the rejected 0.4.0 ears, pupils, cheeks and mouth with two solid cyan
eyes in a fixed graphite enclosure. Gaze moves briefly, settles and holds;
blinks close in four target frames and reopen in six, at unequal intervals.
Task transitions ease eye position and height over nine target frames. Receipt
opens the eyes, work narrows them, and completion gives a gentle squint.
The target remains 30 Hz; actual smoothness requires device observation.

USB/mute, measured battery (unknown remains --%), task status, count and elapsed
time remain visible. The task card is replaced by quiet left-aligned text.
Existing event sounds and volume controls remain; blinks, gaze and work changes
are silent. This is original procedural motion inspired by EMO, not copied
animation/audio assets. Historical versions below describe earlier designs.

## Controls

- UP/DOWN: toggle task details. OK: wave while idle/completed.
- Hold OK: open the 20-state animation gallery. UP/DOWN: browse, OK: exit.
- Gallery is labelled DEMO; live task packets interrupt it.
- After 90 seconds idle, dim to 15%. Any key wakes without triggering an action.
- Missing USB heartbeat for 15 seconds shows disconnected. Waiting/error states
  persist; only a real task-completion event triggers completion.

## Local bridge

Run `tools/buddy/bridge.py` with a Python environment containing pyserial:

```sh
python tools/buddy/bridge.py --port /dev/cu.usbmodem3101
```

The bridge reads recent local `~/.codex/sessions` JSONL metadata. It transmits
state, ASCII project basename, concurrent task count, elapsed seconds and a
sequence number, never prompts, answers or tool output. USB JSON lines are at
115200 baud, version 1; every accepted packet gets a `buddy_ack` response.
Use `--dry-run` without a device or `--project ai-passport` to filter sessions.
Closing the bridge releases USB. It reconnects automatically after unplugging.
No global autostart configuration is installed.

This is a desktop companion using local rollout files, whose format can change.
It classifies known tool names/commands; generic activity is labelled working.
Approval is a supported display/protocol state, but is not automatically
inferred from shell text. The bridge scans today/yesterday in UTC and excludes
turns with no activity for one hour, so it cannot establish the status of older
or abandoned tasks. The device never approves or executes computer actions.
No API key is required.

## Fonts and validation

`assets/fonts/NotoSansSC-Buddy.ttf` is a 223-character Noto Sans SC Medium subset
from Google Fonts (95 printable ASCII and 128 interface Chinese characters).
The OFL license is in the same directory. Generated 16/24 px LVGL fonts use
`lv_font_conv` 1.5.3. Run `npm ci --prefix tools/font-tool` then
`python3 tools/buddy/fonts.py` to regenerate. Adding characters requires updating
the source subset as well; do not silently rely on placeholder glyphs.

`tools/validate.sh` runs repository checks, upstream BSP/demo host regression
tests, Buddy transition/bridge tests, a clean firmware build and image checks.
The key-wake integration contract now covers dimmed-screen wake; no deep sleep
is entered by this application. The linker dead-code flag is platform-specific.
ESP-IDF 5.5.4 is used locally (compatible with manifest >=5.5.3,<5.6.0).
Keep the matching archived ELF for decoding any later crash.

Flashing and serial acknowledgements do not prove the physical screen, Chinese
legibility or key feel; those require observation on the device. Do not claim
those checks from a build alone.

## 3D and sound (0.2.0)

The robot uses perspective projection, back-face culling, depth sorting and flat lighting. Head turns, sway, typing, celebration and sleep poses follow the existing states at a target 10 FPS. No animation bitmaps are allocated; existing task props remain lightweight 2D overlays.

Short square-wave cues cover startup, keys, task receipt, completion, waiting/approval and errors: synthesized 16 kHz / 16-bit PCM with 5 ms envelopes, output volume 25%. Work-state changes and duplicate heartbeats are silent. Hold UP to toggle mute; the footer shows SFX OFF. Sound defaults to on after reboot without Flash writes. A dedicated worker owns the codec, with a single pending cue and bounded chunks; failures disable audio without stopping the UI. Mute does not power down the hardware.

Host tests check PCM amplitude, duration, trailing silence and 10,000 projected frames across all 20 states for bounds and depth ordering. Build/simulation do not establish physical frame rate, appearance or audible quality; these require device observation.

## Volume and pet expressions (0.3.0)

Hold DOWN to enter/leave volume editing. UP/DOWN clicks adjust by 5% within 0–100%; OK exits and 10 seconds without input exits automatically. The existing footer shows percentage and a meter. Nonzero changes play a short preview; zero is silent. Hold UP still toggles mute and restores the previous level. Volume adjustment unmutes. Settings are RAM-only and reset to 25% on reboot; no Flash writes are added. The worker remains the only codec owner; UI actions only submit requests.

Inspired by the expressive eyes shown on the official EMO page (https://living.ai/emo/), the original procedural model gains a larger black face panel and cyan emissive eye geometry, retaining its 3D body and all 20 task states. Idle gaze/blinks, asymmetric thinking eyes, downward reading gaze, focused work eyes, happy arches, error crosses, sad cancellation and sleeping lids are generated locally without official animation assets.

Host regression checks cover volume clamping/extreme input, mute restoration, expression mapping/blinks and 10,000 projected frames. Device flashing/acceptance is reported separately; generated previews are not device captures.

## 0.3.1 — full-screen pet face

The screen is now the pet face: large cyan animated eyes on pure black, without a 3D robot, body, props or frame. State titles/subtitles remain below the eyes; USB/battery and volume controls remain available. Expressions include blinking, looking around, asymmetric thinking eyes, focused eyes, happy arches, red error crosses and sleeping eyelids. This supersedes the 0.3.0 robot presentation. Volume behavior remains unchanged (hold DOWN, UP/DOWN ±5%, OK exit; hold UP mute; RAM-only, reboot default 25%).

Build and host checks are recorded separately from device validation. Screen appearance, volume and buttons still require this build to be flashed and observed.

## 0.4.0 — rounded pet and compact status

A rounded, ear-shaped face rim replaces the unframed square eyes. Oval luminous eyes include inward pupils, highlights, cheeks and a curved mouth. The top row keeps USB/mute status, measured battery percentage and an icon (unknown stays --%; low battery turns red). A compact bottom card shows the task state and active task count/time, or the state subtitle. Detail/gallery/volume modes reuse that card; permanent button hints no longer crowd the face. Existing key actions are preserved.

Application rendering targets 30 Hz instead of 10 Hz; timed eyelid closure/reopening and 300 ms eased gaze/eye-height/color transitions replace hard changes. No new full-screen bitmap buffers are allocated. Actual frame rate and physical appearance remain unverified until this build is flashed.

## System appearance (0.4.9)

Optional protocol field `"theme":"light"` selects a light background and dark teal eyes; `"dark"`, missing or unknown values use the original dark palette. The selected transport controls appearance. Disconnect retains the last palette until reboot (dark by default). Theme changes preserve expression geometry, transition timing and captions. Use desktop 0.4.3 for automatic macOS appearance synchronization. Device appearance and live switching remain pending hardware acceptance.

Device handoff 2026-10-27: firmware 0.4.9 was flashed to ESP32-C3 `98:C3:77:F4:29:4C` at USB 3101 using verified component images (NVS excluded), with all written hashes verified. Bundle full SHA-256: `1f15a4de21783f2cbb3f2c7057c5c94bbd3f346ca2fdbd57fcd4f0a90a24c454`; matching ELF SHA-256: `8589b57d1ed838771dd60899feb78af090bb76003e353c1198339e40322bf777`. Desktop 0.4.3 was opened and recognized actual macOS Light and Dark changes while USB acknowledgments remained confirmed; the original Automatic setting was restored. Screen appearance and BLE theme switching are unverified. Subtitle authorization is currently unavailable; the user elected to leave it unchanged. No startup log was captured in the bounded post-flash observation.

## Light appearance refinement (0.4.10)

Light mode now uses near-black eyes and softer charcoal sleeping eyes. The light background, status colors, eye geometry, animation and dark palette are unchanged. On-device appearance remains pending acceptance for this version.

Device handoff: 0.4.10 was flashed to USB 3101 (MAC `98:C3:77:F4:29:4C`) using verified component images, excluding NVS; all write hashes passed. Bundle: `a540d9bcfb8e3195182b72c36c8ab7ec4c9aa78cfbfc9a151de460cc57522b78`. Desktop 0.4.3 was reopened and fresh USB acknowledgments confirmed. A bounded seven-second serial observation after reset returned no logs; startup-log validation and actual eye appearance remain unverified. Subtitle permissions were left unchanged.

## Dark face with adaptive status area (0.4.11)

The rounded face screen retains its original dark rim, cyan eyes, sleep colors and glow in both appearances. Only the surrounding background, battery, task text and captions follow the host theme. Geometry, expression transitions, controls and saved settings are unchanged.

Device handoff: 0.4.11 flashed to USB 3101 (MAC `98:C3:77:F4:29:4C`); all component write hashes verified, NVS excluded. Full-image SHA-256: `138f0e2ee08b8c9a6151154d3988699d90ecc9131a5099ac49eb58f9ca9ab72e`; matching ELF SHA-256: `58b96ce5f973e77e556deede59625a6e415df775de8d1c1bd6148687ffceba1c`. Build and host gate passed, including identical face geometry/colors across both themes for 26,400 frames. Desktop 0.4.3 was restarted; real macOS Light → Dark → Light changes were reflected in diagnostics with fresh USB acknowledgments. Original Light setting restored. The bounded seven-second startup observation returned no logs. Physical appearance and BLE theme switching remain unverified; subtitle permissions unchanged.
