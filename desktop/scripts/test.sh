#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
sdk_path="${MACOS_SDK_PATH:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk}"
if [[ ! -d "$sdk_path" ]]; then sdk_path="$(xcrun --sdk macosx --show-sdk-path)"; fi
mkdir -p "${project_dir}/build/module-cache"
xcrun swiftc -swift-version 5 -sdk "$sdk_path" -module-cache-path "${project_dir}/build/module-cache" \
    "${project_dir}/Sources/StateMonitor.swift" "${project_dir}/Sources/Interaction.swift" "${project_dir}/Sources/TextReaction.swift" "${project_dir}/tests/StateMonitorTests.swift" -o "${project_dir}/build/StateMonitorTests"
"${project_dir}/build/StateMonitorTests"

xcrun swiftc -swift-version 5 -sdk "$sdk_path" -module-cache-path "${project_dir}/build/module-cache" \
    "${project_dir}/Sources/CaptionBuffer.swift" "${project_dir}/Sources/CaptionRenderer.swift" "${project_dir}/tests/CaptionTests.swift" -framework AppKit -o "${project_dir}/build/CaptionTests"
"${project_dir}/build/CaptionTests" "${project_dir}/build/caption-preview.png"
