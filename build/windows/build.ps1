# Builds the Windows native host with Visual Studio 2022 Build Tools (no IDE needed)
# and, when WiX Toolset 3 is installed, the x86 and x64 MSI packages into dist\.
#
# Usage: powershell -ExecutionPolicy Bypass -File build\windows\build.ps1 [-BuildNumber 0] [-NoMsi]
param(
    [int]$BuildNumber = 0,
    [switch]$NoMsi
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\..\.."
Set-Location $root

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Build Tools not found, install: winget install Microsoft.VisualStudio.2022.BuildTools"
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "C++ build tools (VC.Tools.x86.x64) are not installed" }
$msbuild = Join-Path $vs "MSBuild\Current\Bin\MSBuild.exe"

$sdk = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" | Sort-Object Name | Select-Object -Last 1 -ExpandProperty Name
$ver = @{}
Get-Content VERSION.mk | Where-Object { $_ -match '^(MAJOR|MINOR|RELEASE)_VERSION=(\d+)' } | ForEach-Object { $ver[$Matches[1]] = $Matches[2] }
$version = "$($ver.MAJOR).$($ver.MINOR).$($ver.RELEASE).$BuildNumber"
Write-Host "Building chrome-token-signing $version (SDK $sdk)"

& $msbuild host-windows\host-windows.sln /m /nologo /v:minimal `
    "/p:Configuration=Release;Platform=Win32;PlatformToolset=v143;WindowsTargetPlatformVersion=$sdk" `
    "/p:MAJOR_VERSION=$($ver.MAJOR);MINOR_VERSION=$($ver.MINOR);RELEASE_VERSION=$($ver.RELEASE);BUILD_NUMBER=$BuildNumber"
if ($LASTEXITCODE) { throw "msbuild failed" }
Write-Host "Built host-windows\Release\chrome-token-signing.exe"

if ($NoMsi) { return }
$wix = if ($env:WIX) { Join-Path $env:WIX "bin" } else { $null }
if (-not $wix -or -not (Test-Path "$wix\candle.exe")) {
    Write-Host "WiX Toolset 3 not found (WIX variable), MSI skipped. Install: winget install WiXToolset.WiXToolset"
    return
}
New-Item -ItemType Directory -Force dist | Out-Null
foreach ($platform in "x86", "x64") {
    & "$wix\candle.exe" -nologo host-windows\chrome-token-signing.wxs "-dVERSION=$version" "-dPlatform=$platform" -out "dist\chrome-token-signing.$platform.wixobj"
    if ($LASTEXITCODE) { throw "candle failed" }
    & "$wix\light.exe" -nologo -out "dist\mit-token-signing_$version.$platform.msi" "dist\chrome-token-signing.$platform.wixobj" `
        -ext WixUIExtension "-dWixUILicenseRtf=LICENSE.LGPL.rtf" "-dWixUIDialogBmp=host-windows/dlgbmp.bmp" "-dPlatform=$platform"
    if ($LASTEXITCODE) { throw "light failed" }
    Remove-Item "dist\chrome-token-signing.$platform.wixobj", "dist\mit-token-signing_$version.$platform.wixpdb" -ErrorAction SilentlyContinue
    Write-Host "Built dist\mit-token-signing_$version.$platform.msi"
}
