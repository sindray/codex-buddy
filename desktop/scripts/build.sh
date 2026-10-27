#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
output_dir="${BUDDY_OUTPUT_DIR:-${project_dir}/build}"
app_dir="${output_dir}/Codex Buddy.app"
local_identity_file="${project_dir}/.signing-identity"
local_identity="-"
if [[ -f "$local_identity_file" ]]; then
    local_identity="$(cat "$local_identity_file")"
    if [[ ! "$local_identity" =~ ^[A-Fa-f0-9]{40}$ ]]; then
        echo "Invalid local signing identity: $local_identity_file" >&2
        exit 1
    fi
fi
sign_identity="${BUDDY_SIGN_IDENTITY:-$local_identity}"
# Fail before replacing a working bundle if the requested signing identity is absent.
if [[ "$sign_identity" != "-" ]]; then
    identities="$(security find-identity -v -p codesigning)"
    if ! printf '%s\n' "$identities" | /usr/bin/grep -F -- "$sign_identity" >/dev/null; then
        echo "Signing identity not found: $sign_identity" >&2
        exit 1
    fi
else
    echo "Warning: local ad-hoc signature; Accessibility permission may need renewal after rebuilding." >&2
fi
sdk_path="${MACOS_SDK_PATH:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk}"
if [[ ! -d "$sdk_path" ]]; then sdk_path="$(xcrun --sdk macosx --show-sdk-path)"; fi
mkdir -p "${app_dir}/Contents/MacOS" "${app_dir}/Contents/Resources" "${output_dir}/module-cache"
for arch in arm64 x86_64; do
    xcrun swiftc -swift-version 5 -O -sdk "$sdk_path" -target "${arch}-apple-macos13.0" \
        -module-cache-path "${output_dir}/module-cache" \
        "${project_dir}/Sources/StateMonitor.swift" "${project_dir}/Sources/Transport.swift" "${project_dir}/Sources/Interaction.swift" "${project_dir}/Sources/CaptionBuffer.swift" "${project_dir}/Sources/TextReaction.swift" "${project_dir}/Sources/TextInputMonitor.swift" "${project_dir}/Sources/CaptionRenderer.swift" "${project_dir}/Sources/PetPanel.swift" "${project_dir}/Sources/SetupPanel.swift" "${project_dir}/Sources/main.swift" \
        -framework ApplicationServices -framework Carbon -framework AppKit -framework CoreBluetooth -framework IOKit -framework ServiceManagement \
        -o "${output_dir}/CodexBuddy-${arch}"
done
lipo -create "${output_dir}/CodexBuddy-arm64" "${output_dir}/CodexBuddy-x86_64" -output "${app_dir}/Contents/MacOS/CodexBuddy"
cat > "${app_dir}/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>CodexBuddy</string>
<key>CFBundleIdentifier</key><string>com.johnny.codexbuddy</string>
<key>CFBundleName</key><string>Codex Buddy</string>
<key>CFBundleDisplayName</key><string>Codex Buddy</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.4.7</string>
<key>CFBundleVersion</key><string>11</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>LSUIElement</key><true/>
<key>NSHighResolutionCapable</key><true/>
<key>NSBluetoothAlwaysUsageDescription</key><string>连接你的 AI Passport，通过蓝牙同步本机 Codex 任务状态和表情。</string>
<key>NSBluetoothPeripheralUsageDescription</key><string>连接 AI Passport 同步任务状态。</string>
</dict></plist>
PLIST
xcrun swift -sdk "$sdk_path" -module-cache-path "${output_dir}/module-cache" "${project_dir}/scripts/icon.swift" "${output_dir}"
iconutil -c icns "${output_dir}/AppIcon.iconset" -o "${app_dir}/Contents/Resources/AppIcon.icns"
if [[ "$sign_identity" == "-" ]]; then
    codesign --force --sign - --identifier com.johnny.codexbuddy "$app_dir"
else
    # Local self-signed identities have no Apple timestamp service requirement.
    if [[ "$local_identity" =~ ^[A-Fa-f0-9]{40}$ && "$sign_identity" == "$local_identity" ]]; then
        codesign --force --sign "$sign_identity" --timestamp=none --identifier com.johnny.codexbuddy \
            --requirements "=designated => identifier \"com.johnny.codexbuddy\" and certificate leaf = H\"${local_identity}\"" "$app_dir"
    else
        codesign --force --sign "$sign_identity" --options runtime --timestamp --identifier com.johnny.codexbuddy "$app_dir"
    fi
fi
codesign --verify --deep --strict "$app_dir"
# A distributable bundle, with no reference to this checkout or a Python runtime.
ditto -c -k --sequesterRsrc --keepParent "$app_dir" "${output_dir}/Codex-Buddy-macOS.zip"
echo "Built: $app_dir"
