param(
    [switch] $Tests,
    [switch] $Debug,
    [switch] $Clean
)

$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

# === 1. Find Visual Studio (vswhere) ===

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found: $vswhere"
    exit 1
}

$vsInfo = & $vswhere -latest -prerelease -format json | ConvertFrom-Json
if (-not $vsInfo) {
    Write-Error "No Visual Studio found (vswhere -latest -prerelease)"
    exit 1
}

$vsPath = $vsInfo.installationPath
Write-Host "VS: $($vsInfo.displayName) ($($vsInfo.installationVersion))"
Write-Host "    $vsPath"

$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvarsPath)) {
    Write-Error "vcvars64.bat not found: $vcvarsPath"
    exit 1
}

# === 2. Find cmake ===

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cmake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $cmake) {
        Write-Host "CMake (VS bundled): $cmake"
    }
}
if (-not $cmake -or -not (Test-Path $cmake)) {
    Write-Error "cmake not found. Install cmake or add to PATH."
    exit 1
}
Write-Host "CMake: $cmake"

# === 3. Build config ===

if ($Debug) {
    $preset = "debug"
    $buildDir = "build-debug"
    $config = "Debug"
} else {
    $preset = "default"
    $buildDir = "build"
    $config = "Release"
}

$cmakeExtra = ""
if ($Tests) {
    $cmakeExtra = "-DBUILD_TESTS=ON"
}

# === 4. Clean (optional) ===

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning: $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

# === 5. Build (cmake in cmd with MSVC env) ===

$cmakeCmd = "call `"$vcvarsPath`" > nul 2>&1 && `"$cmake`" --preset $preset $cmakeExtra && `"$cmake`" --build $buildDir --config $config"

Write-Host ""
Write-Host "=== Configure + Build ($preset, $config) ==="
Write-Host ""

cmd /c $cmakeCmd 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit code: $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== Build succeeded! ==="
Write-Host "Output: $buildDir\$config\dock_wmac.exe"

# === 6. Run tests (optional) ===

if ($Tests) {
    Write-Host ""
    Write-Host "=== Running tests ==="

    # Qt bin dir must be in PATH for test executables to find Qt DLLs
    $qtBin = "C:\Qt\6.11.1\msvc2022_64\bin"
    # cmake bin dir for ctest
    $cmakeBin = Split-Path $cmake -Parent

    Push-Location $buildDir
    try {
        $ctestCmd = "call `"$vcvarsPath`" > nul 2>&1 && set PATH=$qtBin;$cmakeBin;%PATH% && ctest -C $config --output-on-failure"
        cmd /c $ctestCmd 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Tests failed (exit code: $LASTEXITCODE)"
            exit $LASTEXITCODE
        }
        Write-Host "All tests passed."
    } finally {
        Pop-Location
    }
}
