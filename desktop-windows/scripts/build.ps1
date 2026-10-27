param([ValidateSet('win-x64','win-arm64')][string]$Runtime = 'win-x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    dotnet run --project Tests/CodexBuddy.Tests.csproj -c Release
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    $out = Join-Path $root "build/$Runtime"
    if (Test-Path $out) { Remove-Item $out -Recurse -Force }
    dotnet publish App/CodexBuddy.Windows.csproj -c Release -r $Runtime --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=None -p:DebugSymbols=false -o $out
    if ($LASTEXITCODE -ne 0) { throw 'Publish failed' }
    Copy-Item README.zh_CN.md "$out/使用说明.md"
    [ordered]@{
        version = '0.1.0'; target = $Runtime; built_on = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
        sdk = (dotnet --version); core_result = 'PASS'; windows_hardware_verified = $false; subtitles = $false
        built_at = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content "$out/build-info.json" -Encoding UTF8
    Compress-Archive -Path "$out/*" -DestinationPath "build/CodexBuddy-0.1.0-$Runtime.zip" -Force
    Get-FileHash "build/CodexBuddy-0.1.0-$Runtime.zip" -Algorithm SHA256
}
finally { Pop-Location }
