<p align="right"><a href="pocket-wreck.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Pocket Wreck

An offline solo demolition arcade for the 240 × 320 badge. The application
renders its own indexed pixel scene; it does not start Wi-Fi, Bluetooth, or LVGL.
Three buildings contain glass, brick, steel, and chain-reacting TNT. Destroying
80% starts a final demolition cascade. The run lasts at most 150 seconds and
starts with five lives. Between buildings choose a longer fury mode, multiball,
or lasers. Best score uses the separate `pocket_wreck` NVS namespace.

## Controls

- UP / DOWN: move the paddle left / right; hold for continuous movement.
- OK: start, launch, activate fury when full, or confirm an upgrade.
- Hold OK for about half a second: pause; hold again while paused: title.
- UP on the title: toggle sound. While paused: UP / DOWN raises / lowers
  volume in 10% steps (0–100%, initially 50%). The setting survives new runs
  until the badge restarts.

The default build selects Pocket Wreck. Configure `-DPASSPORT_WRECK=OFF` to
build the preserved Codex Buddy application. Partitions are unchanged.

## Gentler tuning (0.1.2)

The initial paddle grows from 58 to 72 pixels. Launch and rebound speeds are
about 18% lower. Each building grants one bottom safety rebound; a miss spends
it, and the next building replenishes it. Five lives and the 150-second limit
remain. A brighter ball, launch guide, cyan safety net and demolition progress
bar improve readability; title, upgrade cards and pause use rounded panels.

The 0.1.3 neon arcade revision adds electric purple/cyan/pink lighting, outlined towers, pulsing TNT, connected ball trails, impact rings and fury lightning. The 0.1.2 difficulty tuning remains unchanged.

Glyph generation now uses a shared baseline, keeping horizontal Chinese strokes and punctuation aligned instead of top-aligning every glyph.

## Validation

`tools/validate.sh` includes pure game tests, a complete three-building run,
random input runs, and rendered previews. `tools/wreck_font.py` checks and
regenerates the bitmap font coverage. Hardware logs report frame rate, render
time, input queue latency, and free heap every ten seconds. Acceptance targets
are at least 25 FPS during play, more than 32 KiB free heap, correct Chinese
glyphs, responsive controls, audible effects, and stable repeated restarts.
Logs establish software execution; screen quality, control feel, and speaker
output still require observation on the badge.

Version 0.1.4 centers the building horizontally; block collisions, impact particles and laser column selection share the same origin. Neon styling and the accepted difficulty remain unchanged.
