param(
    [Parameter(Mandatory = $true)]
    [string]$SourceImage,

    [string]$OutputDir = (Join-Path $PSScriptRoot '..\src\as3\aozora\favorites\fallback_icons')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$regions = @(
    @{ Name = 'pistol';    X = 105;  Y = 200; W = 200; H = 125 },
    @{ Name = 'rifle';     X = 405;  Y = 210; W = 280; H = 110 },
    @{ Name = 'heavy';     X = 760;  Y = 195; W = 280; H = 125 },
    @{ Name = 'melee';     X = 1145; Y = 170; W = 230; H = 175 },
    @{ Name = 'explosive'; X = 110;  Y = 445; W = 140; H = 180 },
    @{ Name = 'clothing';  X = 440;  Y = 445; W = 215; H = 180 },
    @{ Name = 'armor';     X = 815;  Y = 440; W = 180; H = 185 },
    @{ Name = 'headwear';  X = 1150; Y = 445; W = 230; H = 180 },
    @{ Name = 'footwear';  X = 100;  Y = 740; W = 185; H = 170 },
    @{ Name = 'medicine';  X = 455;  Y = 725; W = 185; H = 190 },
    @{ Name = 'fooddrink'; X = 825;  Y = 720; W = 180; H = 190 },
    @{ Name = 'utility';   X = 1145; Y = 735; W = 215; H = 165 }
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$source = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $SourceImage))

try {
    foreach ($region in $regions) {
        $cropRect = [System.Drawing.Rectangle]::new($region.X, $region.Y, $region.W, $region.H)
        $crop = $source.Clone($cropRect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $mask = [System.Drawing.Bitmap]::new($crop.Width, $crop.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $minX = $crop.Width
        $minY = $crop.Height
        $maxX = -1
        $maxY = -1

        try {
            for ($y = 0; $y -lt $crop.Height; $y++) {
                for ($x = 0; $x -lt $crop.Width; $x++) {
                    $pixel = $crop.GetPixel($x, $y)
                    $luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                    $alpha = [Math]::Max(0, [Math]::Min(255, [int](($luma - 72.0) * 2.15)))
                    if ($alpha -gt 18) {
                        $mask.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
                        if ($x -lt $minX) { $minX = $x }
                        if ($y -lt $minY) { $minY = $y }
                        if ($x -gt $maxX) { $maxX = $x }
                        if ($y -gt $maxY) { $maxY = $y }
                    }
                }
            }

            if ($maxX -lt $minX -or $maxY -lt $minY) {
                throw "No icon pixels found for $($region.Name)"
            }

            $trimRect = [System.Drawing.Rectangle]::new($minX, $minY, $maxX - $minX + 1, $maxY - $minY + 1)
            $trimmed = $mask.Clone($trimRect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $final = [System.Drawing.Bitmap]::new(128, 128, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $scale = [Math]::Min(112.0 / $trimmed.Width, 112.0 / $trimmed.Height)
                $drawWidth = [Math]::Max(1, [int][Math]::Round($trimmed.Width * $scale))
                $drawHeight = [Math]::Max(1, [int][Math]::Round($trimmed.Height * $scale))
                $drawX = [int][Math]::Floor((128 - $drawWidth) / 2.0)
                $drawY = [int][Math]::Floor((128 - $drawHeight) / 2.0)
                $graphics = [System.Drawing.Graphics]::FromImage($final)
                try {
                    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                    $graphics.DrawImage($trimmed, [System.Drawing.Rectangle]::new($drawX, $drawY, $drawWidth, $drawHeight))
                } finally {
                    $graphics.Dispose()
                }
                $target = Join-Path $OutputDir ($region.Name + '.png')
                $final.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
            } finally {
                $final.Dispose()
                $trimmed.Dispose()
            }
        } finally {
            $mask.Dispose()
            $crop.Dispose()
        }
    }
} finally {
    $source.Dispose()
}

$preview = [System.Drawing.Bitmap]::new(512, 384, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$previewGraphics = [System.Drawing.Graphics]::FromImage($preview)
try {
    $previewGraphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 25, 20))
    $previewGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    for ($i = 0; $i -lt $regions.Count; $i++) {
        $iconPath = Join-Path $OutputDir ($regions[$i].Name + '.png')
        $icon = [System.Drawing.Bitmap]::FromFile($iconPath)
        try {
            $column = $i % 4
            $row = [int][Math]::Floor($i / 4)
            $previewGraphics.DrawImage($icon, 32 + $column * 128, 16 + $row * 128, 80, 80)
        } finally {
            $icon.Dispose()
        }
    }
} finally {
    $previewGraphics.Dispose()
}
$previewPath = Join-Path $OutputDir '_preview.png'
$preview.Save($previewPath, [System.Drawing.Imaging.ImageFormat]::Png)
$preview.Dispose()

Write-Output "OUTPUT=$OutputDir"
Write-Output "PREVIEW=$previewPath"
