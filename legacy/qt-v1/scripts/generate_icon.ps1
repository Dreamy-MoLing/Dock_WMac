# 生成 app.ico — 从 SVG 转多尺寸 ICO
# 优先级: qsvg.exe (Qt) > rsvg-convert (librsvg) > 纯色占位

param(
    [string]$SvgPath = "resources\icons\dock-wmac.svg",
    [string]$OutputIco = "resources\app.ico"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Get-Item $ScriptDir).Parent.FullName
$SvgFull = Join-Path $ProjectRoot $SvgPath
$OutputFull = Join-Path $ProjectRoot $OutputIco
$PngTemp = Join-Path $env:TEMP "dock_wmac_icon_256.png"

# 图标尺寸（Windows 推荐至少包含 16,32,48,256）
$Sizes = @(16, 32, 48, 256)

function New-IconFromPng($png, [int]$sz) {
    $bmp = New-Object System.Drawing.Bitmap($png, $sz, $sz)
    $bmpMs = New-Object System.IO.MemoryStream
    $bmp.Save($bmpMs, [System.Drawing.Imaging.ImageFormat]::Png)
    $data = $bmpMs.ToArray()
    $bmpMs.Close(); $bmp.Dispose()
    return @{ W = $(if ($sz -eq 256) { 0 } else { $sz }); H = $(if ($sz -eq 256) { 0 } else { $sz }); Data = $data }
}

function SvgToPng($svg, $png, $size) {
    # 方案 A: Qt qsvg.exe
    $qsvg = "C:\Qt\6.11.1\msvc2022_64\bin\qsvg.exe"
    if (Test-Path $qsvg) {
        & $qsvg $svg $png
        if ($LASTEXITCODE -eq 0 -and (Test-Path $png)) {
            Write-Host "  [Qt] SVG -> PNG OK: $size x $size"
            return $true
        }
    }

    # 方案 B: rsvg-convert (如果装了 GiMP/Inkscape 等)
    $rsvg = Get-Command rsvg-convert -ErrorAction SilentlyContinue
    if ($rsvg) {
        & $rsvg -w $size -h $size -o $png $svg
        if (Test-Path $png) {
            Write-Host "  [rsvg] SVG -> PNG OK: $size x $size"
            return $true
        }
    }

    return $false
}

# ─── 主流程 ───

Write-Host "生成 app.ico ..."
Write-Host "  源 SVG: $SvgFull"
Write-Host "  输出:   $OutputFull"

Add-Type -AssemblyName System.Drawing

if (Test-Path $SvgFull) {
    # 尝试用工具将 SVG 转为 256x256 PNG
    $ok = SvgToPng $SvgFull $PngTemp 256
    if ($ok) {
        Write-Host "  SVG -> PNG 成功，转为多尺寸 ICO..."
        $png = [System.Drawing.Image]::FromFile($PngTemp)

        # 创建多尺寸 ICO
        $imageDataList = @( (New-IconFromPng $png 16), (New-IconFromPng $png 32), (New-IconFromPng $png 48), (New-IconFromPng $png 256) )
        $png.Dispose()

        $ms = New-Object System.IO.MemoryStream
        $writer = New-Object System.IO.BinaryWriter($ms)
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]4)

        $offset = 6 + 16 * 4
        foreach ($info in $imageDataList) {
            $writer.Write([Byte]$info.W)
            $writer.Write([Byte]$info.H)
            $writer.Write([Byte]0)
            $writer.Write([Byte]0)
            $writer.Write([UInt16]1)
            $writer.Write([UInt16]32)
            $writer.Write([UInt32]$info.Data.Length)
            $writer.Write([UInt32]$offset)
            $offset += $info.Data.Length
        }
        foreach ($info in $imageDataList) {
            $writer.Write($info.Data)
        }

        $writer.Flush()
        [System.IO.File]::WriteAllBytes($OutputFull, $ms.ToArray())
        $ms.Close(); $writer.Close()

        Remove-Item $PngTemp -ErrorAction SilentlyContinue
        Write-Host "  ICO 生成成功 (16, 32, 48, 256 px)"
    } else {
        Write-Host "  SVG 转换工具不可用，使用纯色占位图标"
        $ok = $false
    }
}

if (-not (Test-Path $SvgFull) -or -not $ok) {
    # 纯色占位方案：创建带圆角的彩色方块 ICO
    Write-Host "  生成纯色占位图标..."

    Add-Type -AssemblyName System.Drawing

    function New-IconImage([int]$sz) {
        $bmp = New-Object System.Drawing.Bitmap($sz, $sz)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = 'HighQuality'
        $g.Clear([System.Drawing.Color]::Transparent)

        $c1 = [System.Drawing.Color]::FromArgb(99, 150, 210)
        $c2 = [System.Drawing.Color]::FromArgb(60, 100, 180)
        $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.Point(0, 0)),
            (New-Object System.Drawing.Point($sz, $sz)), $c1, $c2)

        $r = [Math]::Max(4, [int]($sz * 0.22))
        $w = $sz - 2
        $h = $sz - 3
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $path.AddArc(1, 1, $r, $r, 180, 90)
        $path.AddArc(1 + $w - $r, 1, $r, $r, 270, 90)
        $path.AddArc(1 + $w - $r, 1 + $h - $r, $r, $r, 0, 90)
        $path.AddArc(1, 1 + $h - $r, $r, $r, 90, 90)
        $path.CloseFigure()
        $g.FillPath($brush, $path)

        $brush.Dispose(); $path.Dispose(); $g.Dispose()

        $bmpMs = New-Object System.IO.MemoryStream
        $bmp.Save($bmpMs, [System.Drawing.Imaging.ImageFormat]::Png)
        $data = $bmpMs.ToArray()
        $bmpMs.Close(); $bmp.Dispose()
        return @{ W = $(if ($sz -eq 256) { 0 } else { $sz }); H = $(if ($sz -eq 256) { 0 } else { $sz }); Data = $data }
    }

    $ms = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($ms)

    $imageDataList = @( (New-IconImage 16), (New-IconImage 32), (New-IconImage 48), (New-IconImage 256) )

    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]4)  # 4 images

    $offset = 6 + 16 * 4
    foreach ($info in $imageDataList) {
        $writer.Write([Byte]$info.W)
        $writer.Write([Byte]$info.H)
        $writer.Write([Byte]0)
        $writer.Write([Byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$info.Data.Length)
        $writer.Write([UInt32]$offset)
        $offset += $info.Data.Length
    }

    foreach ($info in $imageDataList) {
        $writer.Write($info.Data)
    }

    $writer.Flush()
    [System.IO.File]::WriteAllBytes($OutputFull, $ms.ToArray())
    $ms.Close(); $writer.Close()
    Write-Host "  占位 ICO 生成成功 (16, 32, 48, 256 px)"
}

$fileInfo = Get-Item $OutputFull
Write-Host "完成: $OutputFull ($($fileInfo.Length) bytes)"
