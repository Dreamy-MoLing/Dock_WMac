$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$vcvarsall = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat'
$cmakePath = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$workDir = 'D:\Program\Coding\My\itmes\Cpp\Dock_WMac'
$batFile = Join-Path $workDir '_build_tmp.bat'

# 写临时 batch 文件 - Configure with tests
$batContent = @"
@echo off
chcp 65001 >nul 2>&1
call "$vcvarsall" x64 >nul 2>&1
cd /d "$workDir"
"$cmakePath" --preset default -DBUILD_TESTS=ON
"@
Set-Content -Path $batFile -Value $batContent -Encoding ASCII

Write-Host "=== CMake Configure (with tests) ==="
$proc = Start-Process -FilePath 'cmd.exe' -ArgumentList "/c `"$batFile`"" -WorkingDirectory $workDir -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $workDir 'build_stdout.txt') -RedirectStandardError (Join-Path $workDir 'build_stderr.txt')
Write-Host "Exit code: $($proc.ExitCode)"
Get-Content (Join-Path $workDir 'build_stderr.txt') -ErrorAction SilentlyContinue | Select-Object -Last 10

if ($proc.ExitCode -eq 0) {
    Write-Host "`n=== CMake Build ==="
    $batContent2 = @"
@echo off
chcp 65001 >nul 2>&1
call "$vcvarsall" x64 >nul 2>&1
cd /d "$workDir"
"$cmakePath" --build build --config Release
"@
    Set-Content -Path $batFile -Value $batContent2 -Encoding ASCII
    $proc2 = Start-Process -FilePath 'cmd.exe' -ArgumentList "/c `"$batFile`"" -WorkingDirectory $workDir -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $workDir 'build_stdout.txt') -RedirectStandardError (Join-Path $workDir 'build_stderr.txt')
    Write-Host "Exit code: $($proc2.ExitCode)"

    if ($proc2.ExitCode -eq 0) {
        Write-Host "`n=== Running Tests ==="
        $batContent3 = @"
@echo off
chcp 65001 >nul 2>&1
call "$vcvarsall" x64 >nul 2>&1
cd /d "$workDir\build"
ctest -C Release --output-on-failure
"@
        Set-Content -Path $batFile -Value $batContent3 -Encoding ASCII
        $proc3 = Start-Process -FilePath 'cmd.exe' -ArgumentList "/c `"$batFile`"" -WorkingDirectory $workDir -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $workDir 'build_stdout.txt') -RedirectStandardError (Join-Path $workDir 'build_stderr.txt')
        Write-Host "Exit code: $($proc3.ExitCode)"
        Get-Content (Join-Path $workDir 'build_stdout.txt') -ErrorAction SilentlyContinue
    }
}

# 清理临时文件
Remove-Item $batFile -ErrorAction SilentlyContinue
Remove-Item (Join-Path $workDir 'build_stdout.txt') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $workDir 'build_stderr.txt') -ErrorAction SilentlyContinue
