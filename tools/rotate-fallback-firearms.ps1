param(
    [string]$IconDir = (Join-Path $PSScriptRoot '..\src\as3\aozora\favorites\fallback_icons'),
    [double]$Angle = -38.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$names = @('pistol', 'rifle', 'heavy')
$iconDirResolved = (Resolve-Path -LiteralPath $IconDir).Path

foreach ($name in $names) {
    $path = Join-Path $iconDirResolved ($name + '.png')
    $tempPath = $path + '.rotating.png'
    $source = [System.Drawing.Bitmap]::FromFile($path)
    $target = [System.Drawing.Bitmap]::new(128, 128, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($target)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.TranslateTransform(64.0, 64.0)
            $graphics.RotateTransform($Angle)
            $graphics.TranslateTransform(-64.0, -64.0)
            $graphics.DrawImage($source, 0, 0, 128, 128)
        } finally {
            $graphics.Dispose()
        }
        $target.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $target.Dispose()
        $source.Dispose()
    }
    Move-Item -LiteralPath $tempPath -Destination $path -Force
}

$preview = [System.Drawing.Bitmap]::new(512, 384, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$previewGraphics = [System.Drawing.Graphics]::FromImage($preview)
try {
    $previewGraphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 25, 20))
    $previewGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $allNames = @('pistol', 'rifle', 'heavy', 'melee', 'explosive', 'clothing', 'armor', 'headwear', 'footwear', 'medicine', 'fooddrink', 'utility')
    for ($i = 0; $i -lt $allNames.Count; $i++) {
        $iconPath = Join-Path $iconDirResolved ($allNames[$i] + '.png')
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
$previewPath = Join-Path $iconDirResolved '_preview.png'
$preview.Save($previewPath, [System.Drawing.Imaging.ImageFormat]::Png)
$preview.Dispose()

Write-Output "ANGLE=$Angle"
Write-Output "PREVIEW=$previewPath"
