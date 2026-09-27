[CmdletBinding()]
param(
    [string]$AssetRoot = (Join-Path $PSScriptRoot '..\assets\source\mascot'),
    [string]$SilhouetteRoot = (Join-Path $PSScriptRoot '..\assets\source'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\assets\runtime\Textures\Interface\AozoraFavorites'),
    [ValidateSet('BC3_UNORM', 'R8G8B8A8_UNORM')]
    [string]$TextureFormat = 'BC3_UNORM',
    [switch]$MascotOnly
)

$ErrorActionPreference = 'Stop'
$texconv = (Get-Command texconv -ErrorAction Stop).Source
$AssetRoot = (Resolve-Path -LiteralPath $AssetRoot).Path
$SilhouetteRoot = (Resolve-Path -LiteralPath $SilhouetteRoot).Path
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$stage = Join-Path ([IO.Path]::GetTempPath()) ('aozora-skyui-like-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $OutputDir,$stage | Out-Null
$pendingOutputs = [Collections.Generic.List[object]]::new()

$sets = @(
    @{ Folder = 'Look_01_Chat'; Prefix = 'skyui_like_look_chat_' },
    @{ Folder = 'Look_02_Snack'; Prefix = 'skyui_like_look_snack_' },
    @{ Folder = 'Look_03_Mechanic'; Prefix = 'skyui_like_look_mechanic_' },
    @{ Folder = 'Look_04_Explorer'; Prefix = 'skyui_like_look_explorer_' },
    @{ Folder = 'Look_05_Groom'; Prefix = 'skyui_like_look_groom_' }
)

function Get-PngDimensions([string]$path) {
    $header = [byte[]]::new(26)
    $stream = [IO.File]::OpenRead($path)
    try {
        if ($stream.Read($header, 0, $header.Length) -ne $header.Length -or
            [Text.Encoding]::ASCII.GetString($header, 1, 3) -ne 'PNG') {
            throw "Not a PNG file: $path"
        }
    } finally {
        $stream.Dispose()
    }
    $width = [int](([long]$header[16] * 16777216) + ([long]$header[17] * 65536) +
        ([long]$header[18] * 256) + $header[19])
    $height = [int](([long]$header[20] * 16777216) + ([long]$header[21] * 65536) +
        ([long]$header[22] * 256) + $header[23])
    return @{ Width = $width; Height = $height; ColorType = [int]$header[25] }
}

function Convert-Texture(
    [string]$source,
    [string]$targetName,
    [int]$width = 0,
    [int]$height = 0,
    [int]$mipLevels = 1,
    [switch]$MascotQuality) {
    $arguments = @('-nologo', '-y', '-f', $TextureFormat, '-m', $mipLevels, '-ft', 'dds', '-o', $stage)
    if ($MascotQuality) {
        $arguments += @('-if', 'CUBIC', '-bc', 'd')
    }
    $arguments += @(
        if ($width -gt 0) { '-w'; $width }
        if ($height -gt 0) { '-h'; $height }
        $source
    )
    & $texconv @arguments | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "texconv failed: $source" }
    $converted = Join-Path $stage ((Split-Path -LeafBase $source) + '.dds')
    if (-not (Test-Path -LiteralPath $converted)) { throw "DDS output missing: $converted" }
    $pendingOutputs.Add([pscustomobject]@{
        Source = $converted
        Destination = Join-Path $OutputDir $targetName
    })
}

try {
    $generated = @()
    foreach ($set in $sets) {
        $sourceDir = Join-Path $AssetRoot $set.Folder
        $byNumber = @{}
        foreach ($source in Get-ChildItem -LiteralPath $sourceDir -File -Filter '*.png') {
            $match = [regex]::Match($source.BaseName, '(\d+)$')
            if (-not $match.Success) { continue }
            $number = [int]$match.Groups[1].Value
            if ($number -lt 1 -or $number -gt 10) { continue }
            if ($byNumber.ContainsKey($number)) { throw "Duplicate state $number in $sourceDir" }
            $byNumber[$number] = $source
        }
        for ($number = 1; $number -le 10; ++$number) {
            if (-not $byNumber.ContainsKey($number)) { throw "Missing state $number in $sourceDir" }
            $targetName = $set.Prefix + ('{0:D2}' -f $number) + '.dds'
            $sourceInfo = Get-PngDimensions $byNumber[$number].FullName
            if ($sourceInfo.Width -ne 1024 -or $sourceInfo.Height -ne 1536) {
                throw "Mascot source must be original 1024x1536 (no upscaling): $($byNumber[$number].FullName) is $($sourceInfo.Width)x$($sourceInfo.Height)"
            }
            if ($sourceInfo.ColorType -notin @(4, 6)) {
                throw "Mascot source has no PNG alpha channel: $($byNumber[$number].FullName) colorType=$($sourceInfo.ColorType)"
            }
            Convert-Texture $byNumber[$number].FullName $targetName 1024 1536 0 -MascotQuality
            $generated += $targetName
        }
    }

    if (-not $MascotOnly) {
        $silhouettes = @(
            @{ Source = '剪影素材.png'; Target = 'skyui_like_silhouette_vault_boy.dds' },
            @{ Source = '剪影素材 2.png'; Target = 'skyui_like_silhouette_vault_boy_pipboy.dds' },
            @{ Source = '剪影素材 避难所标志.png'; Target = 'skyui_like_silhouette_vault_tec.dds' }
        )
        foreach ($silhouette in $silhouettes) {
            $source = Join-Path $SilhouetteRoot $silhouette.Source
            if (-not (Test-Path -LiteralPath $source)) { throw "Silhouette not found: $source" }
            Convert-Texture $source $silhouette.Target 512 512
            $generated += $silhouette.Target
        }
    }

    $expectedCount = if ($MascotOnly) { 50 } else { 53 }
    if ($generated.Count -ne $expectedCount) {
        throw "Expected $expectedCount runtime textures, generated $($generated.Count)"
    }
    foreach ($output in $pendingOutputs) {
        Copy-Item -LiteralPath $output.Source -Destination $output.Destination -Force
    }
    Write-Output "OUTPUT=$OutputDir"
    Write-Output "TEXTURE_COUNT=$($generated.Count)"
    Get-ChildItem -LiteralPath $OutputDir -File -Filter '*.dds' | Sort-Object Name |
        ForEach-Object { Write-Output "$($_.Name)`t$($_.Length)`t$((Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash)" }
}
finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
