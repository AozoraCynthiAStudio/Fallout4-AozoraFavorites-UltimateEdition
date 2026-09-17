param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\preview\clothing-footwear-vector-preview.png')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$image = [System.Drawing.Bitmap]::new(720, 360, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($image)
$white = [System.Drawing.Brushes]::White
$dark = [System.Drawing.Pens]::Black

try {
    $graphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 25, 20))
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    function DrawTShirt([System.Drawing.Graphics]$g, [int]$left, [int]$top, [double]$scale) {
        $points = @(
            [System.Drawing.PointF]::new($left + 10*$scale, $top + 6*$scale),
            [System.Drawing.PointF]::new($left + 14*$scale, $top + 3*$scale),
            [System.Drawing.PointF]::new($left + 16*$scale, $top + 7*$scale),
            [System.Drawing.PointF]::new($left + 18*$scale, $top + 7*$scale),
            [System.Drawing.PointF]::new($left + 20*$scale, $top + 3*$scale),
            [System.Drawing.PointF]::new($left + 24*$scale, $top + 6*$scale),
            [System.Drawing.PointF]::new($left + 31*$scale, $top + 11*$scale),
            [System.Drawing.PointF]::new($left + 27*$scale, $top + 18*$scale),
            [System.Drawing.PointF]::new($left + 22*$scale, $top + 15*$scale),
            [System.Drawing.PointF]::new($left + 22*$scale, $top + 29*$scale),
            [System.Drawing.PointF]::new($left + 10*$scale, $top + 29*$scale),
            [System.Drawing.PointF]::new($left + 10*$scale, $top + 15*$scale),
            [System.Drawing.PointF]::new($left + 5*$scale, $top + 18*$scale),
            [System.Drawing.PointF]::new($left + 1*$scale, $top + 11*$scale)
        )
        $g.FillPolygon($white, $points)
        $neck = @(
            [System.Drawing.PointF]::new($left + 13*$scale, $top + 5*$scale),
            [System.Drawing.PointF]::new($left + 16*$scale, $top + 9*$scale),
            [System.Drawing.PointF]::new($left + 19*$scale, $top + 5*$scale),
            [System.Drawing.PointF]::new($left + 18*$scale, $top + 4*$scale),
            [System.Drawing.PointF]::new($left + 16*$scale, $top + 7*$scale),
            [System.Drawing.PointF]::new($left + 14*$scale, $top + 4*$scale)
        )
        $g.FillPolygon([System.Drawing.Brushes]::Black, $neck)
    }

    function DrawSneaker([System.Drawing.Graphics]$g, [int]$left, [int]$top, [double]$scale) {
        $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
        try {
            $path.AddLine([System.Drawing.PointF]::new($left + 3*$scale, $top + 20*$scale), [System.Drawing.PointF]::new($left + 9*$scale, $top + 20*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 9*$scale, $top + 20*$scale), [System.Drawing.PointF]::new($left + 12*$scale, $top + 15*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 12*$scale, $top + 15*$scale), [System.Drawing.PointF]::new($left + 17*$scale, $top + 20*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 17*$scale, $top + 20*$scale), [System.Drawing.PointF]::new($left + 22*$scale, $top + 22*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 22*$scale, $top + 22*$scale), [System.Drawing.PointF]::new($left + 28*$scale, $top + 22*$scale))
            $path.AddBezier([System.Drawing.PointF]::new($left + 28*$scale, $top + 22*$scale), [System.Drawing.PointF]::new($left + 31*$scale, $top + 22*$scale), [System.Drawing.PointF]::new($left + 32*$scale, $top + 24*$scale), [System.Drawing.PointF]::new($left + 32*$scale, $top + 27*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 32*$scale, $top + 27*$scale), [System.Drawing.PointF]::new($left + 32*$scale, $top + 30*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 32*$scale, $top + 30*$scale), [System.Drawing.PointF]::new($left + 4*$scale, $top + 30*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 4*$scale, $top + 30*$scale), [System.Drawing.PointF]::new($left + 2*$scale, $top + 28*$scale))
            $path.AddLine([System.Drawing.PointF]::new($left + 2*$scale, $top + 28*$scale), [System.Drawing.PointF]::new($left + 2*$scale, $top + 24*$scale))
            $path.CloseFigure()
            $g.FillPath($white, $path)
        } finally {
            $path.Dispose()
        }
        $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::Black, [float](1.35*$scale))
        try {
            $g.DrawLine($pen, $left + 9*$scale, $top + 18*$scale, $left + 15*$scale, $top + 20*$scale)
            $g.DrawLine($pen, $left + 10*$scale, $top + 15*$scale, $left + 16*$scale, $top + 18*$scale)
            $g.DrawLine($pen, $left + 6*$scale, $top + 25*$scale, $left + 30*$scale, $top + 25*$scale)
        } finally {
            $pen.Dispose()
        }
    }

    DrawTShirt $graphics 80 35 8.0
    DrawSneaker $graphics 430 35 8.0
    $font = [System.Drawing.Font]::new('Segoe UI', 18, [System.Drawing.FontStyle]::Regular)
    try {
        $graphics.DrawString('T恤 / 上衣', $font, $white, 220, 285)
        $graphics.DrawString('侧面运动鞋', $font, $white, 520, 285)
    } finally {
        $font.Dispose()
    }
} finally {
    $graphics.Dispose()
}
$image.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$image.Dispose()
Write-Output "PREVIEW=$OutputPath"
