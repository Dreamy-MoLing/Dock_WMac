param(
    [string] $Executable = "artifacts\release\Dock_WMac\Dock_WMac_v2.exe",
    [ValidateRange(15, 3600)]
    [int] $DurationSeconds = 300,
    [ValidateRange(1, 120)]
    [int] $WarmupSeconds = 10,
    [ValidateRange(1, 60)]
    [int] $SampleIntervalSeconds = 5,
    [ValidateRange(0, 1024)]
    [double] $MaxPrivateGrowthMB = 16,
    [ValidateRange(0, 4096)]
    [int] $MaxHandleGrowth = 32,
    [ValidateRange(0, 256)]
    [int] $MaxThreadGrowth = 8,
    [ValidateRange(0, 100)]
    [double] $MaxAverageCpuPercent = 1.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDir ".."))
$resolvedExecutable = if ([System.IO.Path]::IsPathRooted($Executable)) {
    [System.IO.Path]::GetFullPath($Executable)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Executable))
}

if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "Dock executable not found: $resolvedExecutable"
}

$existing = Get-Process -Name "Dock_WMac_v2" -ErrorAction SilentlyContinue
if ($existing) {
    $ids = ($existing.Id | Sort-Object) -join ", "
    throw "Close existing Dock_WMac_v2 processes before the stability check. Process IDs: $ids"
}

$logicalProcessorCount = [Math]::Max(1, [Environment]::ProcessorCount)
$bytesPerMB = 1MB
$samples = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[string]]::new()
$process = $null
$startedAtUtc = (Get-Date).ToUniversalTime()
$reportDir = Join-Path $repoRoot "artifacts\validation"
$reportPath = Join-Path $reportDir ("resource-stability-{0}.json" -f (Get-Date -Format "yyyyMMdd-HHmmss"))

function Get-ChildProcesses {
    param([int] $RootProcessId)

    $pending = [System.Collections.Generic.Queue[int]]::new()
    $pending.Enqueue($RootProcessId)
    $descendants = [System.Collections.Generic.List[object]]::new()
    while ($pending.Count -gt 0) {
        $parentId = $pending.Dequeue()
        $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $parentId" -Property ProcessId, ParentProcessId, Name, ExecutablePath)
        foreach ($candidate in $children) {
            $descendants.Add([ordered]@{
                processId = [int]$candidate.ProcessId
                parentProcessId = [int]$candidate.ParentProcessId
                name = [string]$candidate.Name
                executablePath = [string]$candidate.ExecutablePath
            })
            $pending.Enqueue([int]$candidate.ProcessId)
        }
    }
    return @($descendants)
}

function Get-ResourceSample {
    param(
        [System.Diagnostics.Process] $Process,
        [double] $PreviousCpuSeconds,
        [datetime] $PreviousSampleUtc
    )

    if ($Process.HasExited) {
        throw "Dock process exited before the resource check completed (exit code $($Process.ExitCode))."
    }

    $Process.Refresh()
    $sampledAtUtc = (Get-Date).ToUniversalTime()
    $cpuSeconds = $Process.TotalProcessorTime.TotalSeconds
    $wallSeconds = [Math]::Max(0.001, ($sampledAtUtc - $PreviousSampleUtc).TotalSeconds)
    $normalizedCpuPercent = [Math]::Max(
        0.0,
        (($cpuSeconds - $PreviousCpuSeconds) / $wallSeconds) * 100.0 / $logicalProcessorCount)
    return [ordered]@{
        sampledAtUtc = $sampledAtUtc.ToString("o")
        elapsedSeconds = [Math]::Round(($sampledAtUtc - $startedAtUtc).TotalSeconds, 3)
        cpuSeconds = [Math]::Round($cpuSeconds, 6)
        normalizedCpuPercent = [Math]::Round($normalizedCpuPercent, 4)
        workingSetBytes = [int64]$Process.WorkingSet64
        privateBytes = [int64]$Process.PrivateMemorySize64
        handleCount = [int]$Process.HandleCount
        threadCount = [int]$Process.Threads.Count
    }
}

try {
    $process = Start-Process -FilePath $resolvedExecutable -WorkingDirectory (Split-Path -Parent $resolvedExecutable) -PassThru
    Start-Sleep -Seconds $WarmupSeconds

    if ($process.HasExited) {
        throw "Dock process exited during warmup (exit code $($process.ExitCode))."
    }

    $process.Refresh()
    $previousCpuSeconds = $process.TotalProcessorTime.TotalSeconds
    $previousSampleUtc = (Get-Date).ToUniversalTime()
    $baseline = Get-ResourceSample -Process $process -PreviousCpuSeconds $previousCpuSeconds -PreviousSampleUtc $previousSampleUtc
    $samples.Add($baseline)
    $previousCpuSeconds = [double]$baseline.cpuSeconds
    $previousSampleUtc = [datetime]::UtcNow

    $measurementDeadline = [datetime]::UtcNow.AddSeconds($DurationSeconds)
    while ([datetime]::UtcNow -lt $measurementDeadline) {
        $remaining = ($measurementDeadline - [datetime]::UtcNow).TotalSeconds
        $sleepSeconds = [Math]::Min($SampleIntervalSeconds, [Math]::Max(0.1, $remaining))
        Start-Sleep -Milliseconds ([int]($sleepSeconds * 1000))
        $sample = Get-ResourceSample -Process $process -PreviousCpuSeconds $previousCpuSeconds -PreviousSampleUtc $previousSampleUtc
        $samples.Add($sample)
        $previousCpuSeconds = [double]$sample.cpuSeconds
        $previousSampleUtc = [datetime]::UtcNow
    }

    $final = $samples[$samples.Count - 1]
    $privateGrowthBytes = [int64]$final.privateBytes - [int64]$baseline.privateBytes
    $workingSetGrowthBytes = [int64]$final.workingSetBytes - [int64]$baseline.workingSetBytes
    $handleGrowth = [int]$final.handleCount - [int]$baseline.handleCount
    $threadGrowth = [int]$final.threadCount - [int]$baseline.threadCount
    $activitySamples = @($samples | Select-Object -Skip 1)
    $cpuValues = @($activitySamples | ForEach-Object { [double]$_['normalizedCpuPercent'] })
    $averageCpuPercent = if ($activitySamples.Count -gt 0) {
        [double](($cpuValues | Measure-Object -Average).Average)
    } else {
        0.0
    }
    $peakCpuPercent = if ($activitySamples.Count -gt 0) {
        [double](($cpuValues | Measure-Object -Maximum).Maximum)
    } else {
        0.0
    }
    $childProcesses = @(Get-ChildProcesses -RootProcessId $process.Id)

    if ($privateGrowthBytes -gt ($MaxPrivateGrowthMB * $bytesPerMB)) {
        $failures.Add("Private bytes grew by $([Math]::Round($privateGrowthBytes / $bytesPerMB, 2)) MB; limit is $MaxPrivateGrowthMB MB.")
    }
    if ($handleGrowth -gt $MaxHandleGrowth) {
        $failures.Add("Handle count grew by $handleGrowth; limit is $MaxHandleGrowth.")
    }
    if ($threadGrowth -gt $MaxThreadGrowth) {
        $failures.Add("Thread count grew by $threadGrowth; limit is $MaxThreadGrowth.")
    }
    if ($averageCpuPercent -gt $MaxAverageCpuPercent) {
        $failures.Add("Average normalized CPU was $([Math]::Round($averageCpuPercent, 3))%; limit is $MaxAverageCpuPercent%.")
    }
    if ($childProcesses.Count -gt 0) {
        $childNames = ($childProcesses | ForEach-Object { "$($_.name)[$($_.processId)]" } | Sort-Object -Unique) -join ", "
        $failures.Add("Unexpected persistent child process detected: $childNames")
    }

    $report = [ordered]@{
        schemaVersion = 1
        passed = ($failures.Count -eq 0)
        executable = $resolvedExecutable
        processId = $process.Id
        startedAtUtc = $startedAtUtc.ToString("o")
        warmupSeconds = $WarmupSeconds
        measurementSeconds = $DurationSeconds
        sampleIntervalSeconds = $SampleIntervalSeconds
        logicalProcessorCount = $logicalProcessorCount
        thresholds = [ordered]@{
            maxPrivateGrowthBytes = [int64]($MaxPrivateGrowthMB * $bytesPerMB)
            maxHandleGrowth = $MaxHandleGrowth
            maxThreadGrowth = $MaxThreadGrowth
            maxAverageNormalizedCpuPercent = $MaxAverageCpuPercent
            childProcessCount = 0
        }
        baseline = $baseline
        final = $final
        deltas = [ordered]@{
            workingSetBytes = $workingSetGrowthBytes
            privateBytes = $privateGrowthBytes
            handleCount = $handleGrowth
            threadCount = $threadGrowth
        }
        cpu = [ordered]@{
            averageNormalizedPercent = [Math]::Round($averageCpuPercent, 4)
            peakNormalizedPercent = [Math]::Round($peakCpuPercent, 4)
        }
        childProcesses = $childProcesses
        failures = @($failures)
        samples = @($samples)
    }

    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "Resource stability report: $reportPath"
    Write-Host ("Private delta: {0:N2} MB; handles: {1:+0;-0;0}; threads: {2:+0;-0;0}; average CPU: {3:N3}%" -f
        ($privateGrowthBytes / $bytesPerMB), $handleGrowth, $threadGrowth, $averageCpuPercent)

    if ($failures.Count -gt 0) {
        foreach ($failure in $failures) {
            Write-Error $failure
        }
        exit 1
    }
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
}
