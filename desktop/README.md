# Codex Buddy desktop

Native macOS menu bar companion for AI Passport. Requires macOS 13 or later. The universal app contains Apple Silicon and Intel executables and needs no Python. Windows and Linux are not supported yet.

See [中文说明](README.zh_CN.md) for detailed instructions.

## Use

Open `build/Codex Buddy.app` and click the eyes in the menu bar. Power the device and run Codex on this Mac. Auto mode prefers USB and falls back to Bluetooth LE. Allow Bluetooth access when macOS asks; pairing in System Settings is unnecessary. Select a device under Settings if more than one is found.

The main menu keeps play, pause, Settings, Help and quit. Settings contains USB-only/BLE-only modes, device selection, reconnect, the connection guide, Codex directory and login startup. Startup is off by default; move the app into Applications before enabling it. Stop old Python bridges before using the app. Connection locks prevent competing bridge processes. BLE requires firmware 0.4.7 or a compatible Codex Buddy v1 implementation.

Copy `build/Codex-Buddy-macOS.zip` to another Mac, unzip and open. Codex must run on that computer too. The app is ad-hoc signed, without Developer ID signing or notarization; macOS may require allowing it in security settings.

## Interactions (0.2.0)

Click the menu header face or “陪 Buddy 玩…” to open the play panel. Eyes follow the pointer and blink; clicking pets Buddy, with different responses to repeated touches. Wave, praise, curiosity, nap and three random short scenes reuse existing device expressions and sound cues.

Play lasts approximately 3–8 seconds and can be stopped with “回到任务”. The real task remains visible separately. New tasks, completion, urgent states, pause and connection changes cancel play; routine tool transitions do not. Without a device, play stays local and is not queued for reconnection. Device volume settings still apply; no firmware audio resources were added.

`Sources/Interaction.swift` contains the temporary interaction model. `Sources/PetPanel.swift` implements the panel, tracking and smooth eye animation.

## Privacy and diagnostics

Reads recent local records in `~/.codex/sessions`. By default, sends task state, active count, elapsed time, project directory name and sequence number without conversation or file contents. The optional text-caption feature additionally sends a raster of the focused input tail, as detailed below. Long or non-ASCII project names use `Codex task`.

Connected status requires a matching device acknowledgement. Links retry automatically; USB takes priority when restored. Bluetooth busy receipts show that another USB connection owns the device.

Local diagnostic summary: `~/Library/Application Support/CodexBuddy/status.json`. Contains connection state, task state/count and USB ports, without chat contents.

## Build

Install Xcode command line tools, then from the repository root:

```sh
desktop/scripts/test.sh
desktop/scripts/build.sh
```

Sources are in `Sources/`: `main.swift` (menu/preferences), `Transport.swift` (USB/CoreBluetooth), and `StateMonitor.swift` (task monitoring/protocol). Tests are in `tests/`. Outputs go into `build/`.

Override the SDK with `MACOS_SDK_PATH`. The build prefers the locally installed macOS 26.5 SDK, falling back to the system default. Login startup uses Apple's `SMAppService` and is subject to system approval.

## First-run setup (0.3.0)

The connection guide checks the Codex source and device separately. Select a Codex home directory or its `sessions` folder if auto-detection fails. Saved selection overrides an absolute `CODEX_HOME` inherited by the process, then falls back to `~/.codex/sessions`. Finder launches usually do not inherit terminal environment variables. Switching the source clears cached tasks; invalid folder selections keep the previous setting.

The guide distinguishes a missing/unreadable directory from no records in the last two UTC days. It can select and remember a Bluetooth device and offers optional login startup. Completion requires recent records and a device ACK; skipping keeps setup pending for the next launch. A remembered device identifier is not authenticated pairing. Reopen the guide from Settings at any time.

The 0.3.0 package is still locally signed, not Developer ID signed or notarized. Cross-Mac installation, login restart, and the new setup flow over BLE require release acceptance. See `RELEASE.zh_CN.md` for the release checklist.

## Text reactions and live captions (0.4.1)

Version 0.4.6 uses a 17 px PingFang medium raster, quantizing grayscale coverage into monochrome pixels with LCD subpixel smoothing disabled. A fixed font baseline prevents punctuation and emoji from shifting existing text vertically. Overflow removes complete grapheme clusters and prefixes an ellipsis while retaining the tail. Firmware 0.4.14 uses a quiet footer: a muted 16 px status on the left and an elapsed timer on the right during active tasks. The second row is reserved for captions and stays empty when no caption is present; volume, detail, and demo modes retain their controls. The dot, task count, divider, and routine description are removed. Both rows use a 24 px inset; captions start at y=289 with a 13 px bottom margin. The 432-byte monochrome protocol is unchanged; older firmware can receive the updated raster. Edits settle for at least 300 ms before publication; continuous typing publishes within approximately 1–1.5 seconds at the 500 ms poll cadence. Selection temporarily retains the last caption, caret-only navigation does not publish old text, and captions expire five seconds after the last edit. This is snapshot coalescing, not guaranteed IME composition detection; behavior depends on the editor's Accessibility support.

Enable Settings → 文字互动与字幕, then allow Codex Buddy in macOS Privacy & Security → Accessibility. The feature is off by default and can be disabled anytime. Captions require firmware 0.4.8; older firmware retains task and expression support.

The helper polls up to 64 UTF-16 units before the caret of a supported focused editor every 0.5 seconds. Switching fields establishes a baseline without displaying existing text. Subsequent edits show the latest line at the bottom of the device, following the end of long input and fading after about five seconds without edits. Eyes, task state, and battery remain visible; volume, detail, and gallery modes take precedence. The Mac renders a 192×18 monochrome raster over USB/BLE, avoiding extra Chinese device fonts.

哈哈 / 加油 / 搞定 trigger praise, 好困 / 晚安 trigger a nap, ？？ / 为什么 trigger curiosity, and 你好 / hi buddy trigger a greeting. A 12-second cooldown applies, and urgent task states and existing interactions take priority. Text and rasters stay in memory, are excluded from diagnostics, and are never sent to a cloud service. Caption images are transmitted to your device. Secure fields and system secure-input sessions are skipped. Pausing, disconnecting, disabling, or losing permission stops collection and clears local buffers; device captions also expire independently.

Only editors exposing Accessibility text-range APIs are supported. Terminals, custom editors, and some web pages may be unsupported. Uncommitted IME candidates are not guaranteed. Bluetooth latency depends on negotiated write size and connection quality; 0.5 seconds is the polling interval, not an end-to-end latency guarantee.

The default task monitor does not transmit conversation content. Opting into text captions additionally transmits a raster of the focused input tail as described above.

### Caption permission troubleshooting (0.4.2)

If task sync works but captions disappear after a local app update, macOS may still show the old Accessibility entry enabled while the new process is untrusted. Turn Codex Buddy off and on again in the system permission list, then restart the helper if requested. Local ad-hoc signing can require this after executable changes. Diagnostics include only `textEnabled`, `textPermission`, `textInputStatus` and `captionReady`, never the input text or raster. `permissionRequired` identifies the permission gate; it does not indicate a device failure.

## System appearance

Version 0.4.3 follows macOS Light/Dark appearance automatically and sends `theme` over both USB and BLE. Requires device firmware 0.4.9 for the light palette. No extra permission or menu is needed. Paused synchronization applies the current appearance when resumed.

### Keeping caption permission across updates

Local builds default to an ad-hoc signature, whose designated requirement is tied to that particular executable. A permission-list toggle can therefore remain on while a rebuilt process is untrusted. Refresh only the Codex Buddy entry in macOS Privacy & Security → Accessibility (called Device Control and Data Access on some macOS versions); if toggling fails, remove that entry and add the exact app you run, then restart it. System authentication may be required. Keep one installed copy at a stable location. Do not reset other apps or edit the privacy database.

For releases, retain bundle ID `com.johnny.codexbuddy` and a consistent Developer ID signing identity. Set `BUDDY_SIGN_IDENTITY` to the certificate name or SHA-1 identity when running `scripts/build.sh`; an unavailable identity fails before replacing the bundle, without falling back to ad-hoc signing. The first transition from ad-hoc to certificate signing still requires authorization. Certificate signing support does not mean this build is notarized, and permission continuity still requires upgrade testing. See [Apple TN3127](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements).

## Idle display protection (helper 0.4.7 / firmware 0.4.15)

Settings → 空闲自动熄屏 offers 1, 3, 5 (default), 10, or 30 minutes and Never. The helper remembers the choice and sends `screenOffSeconds` over USB and BLE; firmware stores changed values in NVS. Missing/invalid fields keep the device setting, so older helpers remain compatible. Valid wire values are 0 (never) or 60–3600 seconds.

After 90 seconds idle the backlight dims to 15% and animation updates drop from approximately 30 to 5 per second. At the configured timeout the backlight turns off and application rendering stops. Transport monitoring remains active; this is display power saving, not chip deep sleep. Active tasks, approval/waiting/error states, and demo playback stay awake. A new task/state, new caption, connection recovery, or a device button wakes the screen; the first button only wakes it. Repeated heartbeats do not reset the timer. Disconnected devices also time out. Changing the timeout wakes the display and starts a fresh interval.

Build and short device checks do not establish 24/7 stability, panel lifetime, or measured power savings.

Double-click the device OK button to manually turn off the screen. Press any function button to wake; the wake gesture is consumed. Manual screen lock overrides task, caption, and connection wake events until a button is pressed. USB/BLE monitoring continues. Single-click and long-press actions remain unchanged when awake. The hardware power button retains its original power function.
