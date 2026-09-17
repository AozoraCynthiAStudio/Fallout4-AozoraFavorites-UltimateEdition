param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\preview\fis-style-fallback-preview.png')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$image = [System.Drawing.Bitmap]::new(960, 430, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($image)
$white = [System.Drawing.Brushes]::White
$black = [System.Drawing.Pens]::Black

function P([double]$x, [double]$y) { return [System.Drawing.PointF]::new([float]$x, [float]$y) }

function DrawTShirt([System.Drawing.Graphics]$g, [double]$ox, [double]$oy, [double]$s) {
    $points = @(
        (P ($ox + 9*$s) ($oy + 6*$s)), (P ($ox + 13*$s) ($oy + 2*$s)),
        (P ($ox + 16*$s) ($oy + 7*$s)), (P ($ox + 19*$s) ($oy + 2*$s)),
        (P ($ox + 23*$s) ($oy + 6*$s)), (P ($ox + 31*$s) ($oy + 11*$s)),
        (P ($ox + 27*$s) ($oy + 19*$s)), (P ($ox + 22*$s) ($oy + 16*$s)),
        (P ($ox + 22*$s) ($oy + 30*$s)), (P ($ox + 10*$s) ($oy + 30*$s)),
        (P ($ox + 10*$s) ($oy + 16*$s)), (P ($ox + 5*$s) ($oy + 19*$s)),
        (P ($ox + 1*$s) ($oy + 11*$s))
    )
    $g.FillPolygon($white, $points)
    $neck = @(
        (P ($ox + 13*$s) ($oy + 4*$s)), (P ($ox + 16*$s) ($oy + 10*$s)),
        (P ($ox + 19*$s) ($oy + 4*$s)), (P ($ox + 18*$s) ($oy + 3*$s)),
        (P ($ox + 16*$s) ($oy + 7*$s)), (P ($ox + 14*$s) ($oy + 3*$s))
    )
    $g.FillPolygon([System.Drawing.Brushes]::Black, $neck)
}

function DrawBoot([System.Drawing.Graphics]$g, [double]$ox, [double]$oy, [double]$s) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    try {
        $path.AddLine((P ($ox + 4*$s) ($oy + 20*$s)), (P ($ox + 9*$s) ($oy + 19*$s)))
        $path.AddLine((P ($ox + 9*$s) ($oy + 19*$s)), (P ($ox + 13*$s) ($oy + 15*$s)))
        $path.AddLine((P ($ox + 13*$s) ($oy + 15*$s)), (P ($ox + 15*$s) ($oy + 9*$s)))
        $path.AddLine((P ($ox + 15*$s) ($oy + 9*$s)), (P ($ox + 20*$s) ($oy + 9*$s)))
        $path.AddLine((P ($ox + 20*$s) ($oy + 9*$s)), (P ($ox + 21*$s) ($oy + 16*$s)))
        $path.AddLine((P ($ox + 21*$s) ($oy + 16*$s)), (P ($ox + 25*$s) ($oy + 20*$s)))
        $path.AddLine((P ($ox + 25*$s) ($oy + 20*$s)), (P ($ox + 29*$s) ($oy + 22*$s)))
        $path.AddBezier((P ($ox + 29*$s) ($oy + 22*$s)), (P ($ox + 31*$s) ($oy + 23*$s)), (P ($ox + 32*$s) ($oy + 25*$s)), (P ($ox + 31*$s) ($oy + 28*$s)))
        $path.AddLine((P ($ox + 31*$s) ($oy + 28*$s)), (P ($ox + 31*$s) ($oy + 30*$s)))
        $path.AddLine((P ($ox + 31*$s) ($oy + 30*$s)), (P ($ox + 16*$s) ($oy + 30*$s)))
        $path.AddLine((P ($ox + 16*$s) ($oy + 30*$s)), (P ($ox + 14*$s) ($oy + 24*$s)))
        $path.AddLine((P ($ox + 14*$s) ($oy + 24*$s)), (P ($ox + 13*$s) ($oy + 30*$s)))
        $path.AddLine((P ($ox + 13*$s) ($oy + 30*$s)), (P ($ox + 6*$s) ($oy + 30*$s)))
        $path.AddLine((P ($ox + 6*$s) ($oy + 30*$s)), (P ($ox + 3*$s) ($oy + 28*$s)))
        $path.AddLine((P ($ox + 3*$s) ($oy + 28*$s)), (P ($ox + 3*$s) ($oy + 24*$s)))
        $path.AddLine((P ($ox + 3*$s) ($oy + 24*$s)), (P ($ox + 4*$s) ($oy + 20*$s)))
        $path.CloseFigure()
        $g.FillPath($white, $path)
    } finally { $path.Dispose() }
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::Black, [float](1.25*$s))
    try {
        $g.DrawLine($pen, [float]($ox + 15*$s), [float]($oy + 10*$s), [float]($ox + 20*$s), [float]($oy + 10*$s))
        $g.DrawLine($pen, [float]($ox + 7*$s), [float]($oy + 27*$s), [float]($ox + 30*$s), [float]($oy + 27*$s))
    } finally { $pen.Dispose() }
}

function DrawRifle([System.Drawing.Graphics]$g, [double]$ox, [double]$oy, [double]$s) {
    $stock = @(
        (P ($ox + 1*$s) ($oy + 19*$s)), (P ($ox + 7*$s) ($oy + 14*$s)),
        (P ($ox + 13*$s) ($oy + 17*$s)), (P ($ox + 11*$s) ($oy + 20*$s)),
        (P ($ox + 7*$s) ($oy + 24*$s)), (P ($ox + 4*$s) ($oy + 27*$s)),
        (P ($ox + 1*$s) ($oy + 25*$s))
    )
    $g.FillPolygon($white, $stock)
    $upper = @(
        (P ($ox + 8*$s) ($oy + 15*$s)), (P ($ox + 13*$s) ($oy + 11*$s)),
        (P ($ox + 19*$s) ($oy + 15*$s)), (P ($ox + 16*$s) ($oy + 20*$s)),
        (P ($ox + 11*$s) ($oy + 19*$s))
    )
    $g.FillPolygon($white, $upper)
    $mag = @(
        (P ($ox + 14*$s) ($oy + 18*$s)), (P ($ox + 18*$s) ($oy + 20*$s)),
        (P ($ox + 18*$s) ($oy + 24*$s)), (P ($ox + 16*$s) ($oy + 29*$s)),
        (P ($ox + 13*$s) ($oy + 30*$s)), (P ($ox + 11*$s) ($oy + 27*$s)),
        (P ($ox + 14*$s) ($oy + 23*$s)), (P ($ox + 12*$s) ($oy + 20*$s))
    )
    $g.FillPolygon($white, $mag)
    $grip = @(
        (P ($ox + 10*$s) ($oy + 17*$s)), (P ($ox + 14*$s) ($oy + 20*$s)),
        (P ($ox + 12*$s) ($oy + 27*$s)), (P ($ox + 9*$s) ($oy + 26*$s))
    )
    $g.FillPolygon($white, $grip)
    $g.FillPolygon([System.Drawing.Brushes]::Black, @(
        (P ($ox + 7*$s) ($oy + 18*$s)), (P ($ox + 10*$s) ($oy + 16*$s)),
        (P ($ox + 11*$s) ($oy + 19*$s)), (P ($ox + 9*$s) ($oy + 21*$s))
    ))
}

try {
    $graphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 25, 20))
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    DrawTShirt $graphics 45 45 7.0
    DrawBoot $graphics 365 45 7.0
    DrawRifle $graphics 685 45 7.0
    $font = [System.Drawing.Font]::new('Segoe UI', 19, [System.Drawing.FontStyle]::Regular)
    try {
        $graphics.DrawString('97TShirt 风格', $font, $white, 95, 285)
        $graphics.DrawString('高跟鞋风格', $font, $white, 440, 285)
        $graphics.DrawString('AK 步枪风格', $font, $white, 755, 285)
    } finally { $font.Dispose() }
    $smallFont = [System.Drawing.Font]::new('Segoe UI', 13, [System.Drawing.FontStyle]::Regular)
    try { $graphics.DrawString('底部为实际 16×16 显示比例（放大 4 倍）', $smallFont, $white, 300, 340) } finally { $smallFont.Dispose() }
    DrawTShirt $graphics 300 365 2.0
    DrawBoot $graphics 435 365 2.0
    DrawRifle $graphics 570 365 2.0
} finally {
    $graphics.Dispose()
}
$image.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$image.Dispose()
Write-Output "PREVIEW=$OutputPath"
