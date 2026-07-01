param(
    [string] $Configuration = "Release",
    [string] $Platform = "x64",
    [string] $PlatformToolset = "v145",
    [string] $MSBuildPath = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
$solution = Join-Path $repoRoot "Dock_WMac_v2.sln"
$buildOut = Join-Path $repoRoot "build\v2\$Platform\$Configuration"
$artifactRoot = Join-Path $repoRoot "artifacts\release"
$appFolder = Join-Path $artifactRoot "Dock_WMac"

if (-not (Test-Path $MSBuildPath)) {
    throw "MSBuild not found: $MSBuildPath"
}

& $MSBuildPath $solution /restore "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$resolvedRepo = [System.IO.Path]::GetFullPath($repoRoot)
$resolvedArtifact = [System.IO.Path]::GetFullPath($appFolder)
if (-not $resolvedArtifact.StartsWith($resolvedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean artifact path outside repo: $resolvedArtifact"
}

if (Test-Path $appFolder) {
    Remove-Item -LiteralPath $appFolder -Recurse -Force
}

New-Item -ItemType Directory -Path $appFolder | Out-Null
Copy-Item -Path (Join-Path $buildOut "*") -Destination $appFolder -Recurse -Force

$licenses = Join-Path $appFolder "licenses"
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination (Join-Path $licenses "Dock_WMac-LICENSE.txt") -Force

@"
Dock_WMac v1.0.0 Taskbar Dock Release

Run:
  Dock_WMac_v2.exe

Self-check:
  Dock_WMac_v2.exe --self-check

Packaging:
  Unpackaged Win32 with Windows App SDK bootstrapper.
  Runtime files are kept in this app folder.
  Normal user state is stored under %LOCALAPPDATA%\Dock_WMac.
  App-local data/ is reserved for an explicit future portable mode.
"@ | Set-Content -LiteralPath (Join-Path $appFolder "README_RELEASE.txt") -Encoding UTF8

Write-Host "Packaged $Configuration $Platform release to $appFolder"
