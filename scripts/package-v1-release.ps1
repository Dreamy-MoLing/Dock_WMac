param(
    [string] $Configuration = "Release",
    [string] $Platform = "x64",
    [string] $PlatformToolset = "v145",
    [string] $MSBuildPath = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe",
    [string] $Version = "v1.0.0",
    [switch] $SkipBuild,
    [switch] $SkipValidation,
    [switch] $IncludeSymbols,
    [switch] $NoZip
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
$solution = Join-Path $repoRoot "Dock_WMac_v2.sln"
$buildOut = Join-Path $repoRoot "build\v2\$Platform\$Configuration"
$artifactRoot = Join-Path $repoRoot "artifacts\release"
$appFolder = Join-Path $artifactRoot "Dock_WMac"
$manifestPath = Join-Path $appFolder "release-manifest.json"
$safeVersion = $Version -replace '[\\/:*?"<>|]', "-"
$zipPath = Join-Path $artifactRoot ("Dock_WMac-{0}-windows-{1}.zip" -f $safeVersion, $Platform)
$resolvedRepo = [System.IO.Path]::GetFullPath($repoRoot)

function Invoke-ValidationCommand {
    param(
        [string] $Path,
        [string[]] $Arguments,
        [string] $Label
    )

    $stdoutFile = [System.IO.Path]::GetTempFileName()
    $stderrFile = [System.IO.Path]::GetTempFileName()
    try {
        $startParams = @{
            FilePath = $Path
            Wait = $true
            PassThru = $true
            WindowStyle = "Hidden"
            RedirectStandardOutput = $stdoutFile
            RedirectStandardError = $stderrFile
        }
        if ($Arguments.Count -gt 0) {
            $startParams.ArgumentList = $Arguments
        }

        $process = Start-Process @startParams
        $stdout = Get-Content -LiteralPath $stdoutFile -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host $stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Warning $stderr.TrimEnd()
        }

        return [ordered]@{
            command = $Label
            exitCode = $process.ExitCode
        }
    }
    finally {
        Remove-Item -LiteralPath $stdoutFile, $stderrFile -Force -ErrorAction SilentlyContinue
    }
}

if ((-not $SkipBuild) -and (-not (Test-Path $MSBuildPath))) {
    throw "MSBuild not found: $MSBuildPath"
}

if (-not $SkipBuild) {
    if (Test-Path $buildOut) {
        $resolvedBuildOut = [System.IO.Path]::GetFullPath($buildOut)
        if (-not $resolvedBuildOut.StartsWith($resolvedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean build output outside repo: $resolvedBuildOut"
        }
        Remove-Item -LiteralPath $buildOut -Recurse -Force
    }

    & $MSBuildPath $solution /restore "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path $buildOut)) {
    throw "Build output not found: $buildOut"
}

$resolvedArtifact = [System.IO.Path]::GetFullPath($appFolder)
if (-not $resolvedArtifact.StartsWith($resolvedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean artifact path outside repo: $resolvedArtifact"
}
$resolvedZip = [System.IO.Path]::GetFullPath($zipPath)
if (-not $resolvedZip.StartsWith($resolvedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to write zip outside repo: $resolvedZip"
}

if (Test-Path $appFolder) {
    Remove-Item -LiteralPath $appFolder -Recurse -Force
}
if ((-not $NoZip) -and (Test-Path $zipPath)) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Path $appFolder | Out-Null
$releaseFiles = @("Dock_WMac_v2.exe", "Dock_WMac_v2.pri")
if ($IncludeSymbols) {
    $releaseFiles += "Dock_WMac_v2.pdb"
}
foreach ($releaseFile in $releaseFiles) {
    $sourcePath = Join-Path $buildOut $releaseFile
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Required build output missing: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $appFolder $releaseFile) -Force
}

$licenses = Join-Path $appFolder "licenses"
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination (Join-Path $licenses "Dock_WMac-LICENSE.txt") -Force

$requiredFiles = @(
    "Dock_WMac_v2.exe",
    "Dock_WMac_v2.pri"
)
foreach ($required in $requiredFiles) {
    $requiredPath = Join-Path $appFolder $required
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required release file missing: $requiredPath"
    }
}

$forbiddenReleaseFiles = Get-ChildItem -LiteralPath $appFolder -Recurse -File |
    Where-Object {
        $relativePath = $_.FullName.Substring($resolvedArtifact.Length + 1).Replace("\", "/")
        ($relativePath -like "*WebView2*") -or
        ($relativePath -like "Microsoft.Web.*") -or
        ($relativePath -like "Microsoft.UI*") -or
        ($relativePath -like "Microsoft.WindowsAppRuntime*") -or
        ($relativePath -like "*onnxruntime*") -or
        ($relativePath -like "DirectML*")
    }
if ($forbiddenReleaseFiles) {
    $names = ($forbiddenReleaseFiles | ForEach-Object {
        $_.FullName.Substring($resolvedArtifact.Length + 1).Replace("\", "/")
    }) -join ", "
    throw "Forbidden browser/WebView runtime files in release package: $names"
}

@"
Dock_WMac $Version Taskbar Dock Release

Run:
  Dock_WMac_v2.exe

Self-check:
  Dock_WMac_v2.exe --self-check

Packaging:
  Native x64 unpackaged Win32 with a static Visual C++ runtime.
  No Windows App Runtime, WinUI, WebView, or ML runtime is required.
  Runtime files are kept in this app folder.
  Normal user state is stored under %LOCALAPPDATA%\Dock_WMac.
  The Dock is displayed on the Windows primary display only.
  App-local data/ is reserved for an explicit future portable mode.
"@ | Set-Content -LiteralPath (Join-Path $appFolder "README_RELEASE.txt") -Encoding UTF8

$validation = [ordered]@{
    skipped = [bool]$SkipValidation
    forbiddenRuntimePayloadsAbsent = $true
    commands = @()
}

if (-not $SkipValidation) {
    $testExe = Join-Path $buildOut "Dock_WMac_v2_tests.exe"
    if (-not (Test-Path -LiteralPath $testExe)) {
        throw "Built test executable not found: $testExe"
    }

    $commands = @(
        @{ path = $testExe; args = @(); label = "Dock_WMac_v2_tests.exe" },
        @{ path = (Join-Path $appFolder "Dock_WMac_v2.exe"); args = @("--self-check"); label = "Dock_WMac_v2.exe --self-check" },
        @{ path = (Join-Path $appFolder "Dock_WMac_v2.exe"); args = @("--dump-dock-state"); label = "Dock_WMac_v2.exe --dump-dock-state" },
        @{ path = (Join-Path $appFolder "Dock_WMac_v2.exe"); args = @("--dump-resource-metrics"); label = "Dock_WMac_v2.exe --dump-resource-metrics" }
    )

    foreach ($command in $commands) {
        $result = Invoke-ValidationCommand -Path $command.path -Arguments $command.args -Label $command.label
        $validation.commands += $result
        if ($result.exitCode -ne 0) {
            throw "Release validation failed: $($command.label) exited $($result.exitCode)"
        }
    }
}

$files = Get-ChildItem -LiteralPath $appFolder -Recurse -File |
    Where-Object { $_.FullName -ne $manifestPath } |
    Sort-Object FullName |
    ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($resolvedArtifact.Length + 1).Replace("\", "/")
            bytes = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }

$manifest = [ordered]@{
    product = "Dock_WMac"
    version = $Version
    configuration = $Configuration
    platform = $Platform
    platformToolset = $PlatformToolset
    deploymentMode = "native-xcopy"
    windowsAppRuntimeRequired = $false
    visualCppRedistributableRequired = $false
    packagedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    appFolderName = "Dock_WMac"
    entryPoint = "Dock_WMac_v2.exe"
    symbolsIncluded = [bool]$IncludeSymbols
    validation = $validation
    files = $files
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

if (-not $NoZip) {
    Compress-Archive -Path (Join-Path $appFolder "*") -DestinationPath $zipPath -Force
    $zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "Release zip: $zipPath"
    Write-Host "Release zip SHA256: $zipHash"
}

Write-Host "Packaged $Configuration $Platform release to $appFolder"
