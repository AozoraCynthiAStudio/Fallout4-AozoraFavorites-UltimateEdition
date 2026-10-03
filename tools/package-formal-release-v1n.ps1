[CmdletBinding()]
param(
    [string]$Version = '1.0.3',
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\package')
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$nativeDll = (Resolve-Path (Join-Path $root 'src\native\build\windows\x64\release\AozoraFavoritesSWF.dll')).Path
$swf = (Resolve-Path (Join-Path $root 'build\AozoraFavoritesMenu.swf')).Path
$pluginEsp = (Resolve-Path (Join-Path $root 'Data\AozoraFavorites.esp')).Path
$textureRoot = Join-Path $root 'assets\runtime\Textures\Interface\AozoraFavorites'
$mcmRoot = Join-Path $root 'mcm'
$translationRoot = Join-Path $root 'translations'
$legacyScripts = Join-Path $root 'assets\runtime\Scripts'
$outputRootResolved = [IO.Path]::GetFullPath($OutputRoot)
$coreDir = Join-Path $outputRootResolved ("AozoraFavorites Ultimate Edition v" + $Version)
$chsDir = Join-Path $outputRootResolved ("AozoraFavorites Ultimate Edition CHS Patch v" + $Version)

foreach ($dir in @($coreDir, $chsDir)) {
    if (Test-Path -LiteralPath $dir) {
        throw "Refusing to overwrite existing release directory: $dir"
    }
}

$textures = @(Get-ChildItem -LiteralPath $textureRoot -File -Filter '*.dds')
if ($textures.Count -ne 53) { throw "Expected 53 runtime textures, found $($textures.Count)" }

$coreDirs = @(
    'F4SE\Plugins', 'Interface', 'Interface\Translations',
    'Textures\Interface\AozoraFavorites', 'Scripts',
    'MCM\Config\AozoraFavorites', 'MCM\Settings'
)
foreach ($dir in $coreDirs) {
    New-Item -ItemType Directory -Force -Path (Join-Path $coreDir $dir) | Out-Null
}
New-Item -ItemType Directory -Force -Path (Join-Path $chsDir 'Interface\Translations') | Out-Null

Copy-Item -LiteralPath $nativeDll -Destination (Join-Path $coreDir 'F4SE\Plugins\AozoraFavoritesSWF.dll')
Copy-Item -LiteralPath $swf -Destination (Join-Path $coreDir 'Interface\AozoraFavoritesMenu.swf')
Copy-Item -LiteralPath $pluginEsp -Destination (Join-Path $coreDir 'AozoraFavorites.esp')
Copy-Item -LiteralPath (Join-Path $translationRoot 'AozoraFavorites_en.txt') -Destination (Join-Path $coreDir 'Interface\Translations\AozoraFavorites_en.txt')
foreach ($texture in $textures) {
    Copy-Item -LiteralPath $texture.FullName -Destination (Join-Path $coreDir 'Textures\Interface\AozoraFavorites')
}
foreach ($scriptName in @('AozoraMenuNative.pex', 'AozoraMenuRun.pex')) {
    $script = Join-Path $legacyScripts $scriptName
    if (Test-Path -LiteralPath $script) {
        Copy-Item -LiteralPath $script -Destination (Join-Path $coreDir 'Scripts')
    }
}
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Config\AozoraFavorites\config.json') -Destination (Join-Path $coreDir 'MCM\Config\AozoraFavorites\config.json')
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Config\AozoraFavorites\keybinds.json') -Destination (Join-Path $coreDir 'MCM\Config\AozoraFavorites\keybinds.json')
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Settings\AozoraFavorites.ini') -Destination (Join-Path $coreDir 'MCM\Settings\AozoraFavorites.ini')

Copy-Item -LiteralPath (Join-Path $translationRoot 'AozoraFavorites_cn.txt') -Destination (Join-Path $chsDir 'Interface\Translations\AozoraFavorites_en.txt')

function Write-Manifest([string]$label, [string]$dir) {
    $files = Get-ChildItem -LiteralPath $dir -File -Recurse
    Write-Output "RELEASE=$label"
    Write-Output "DIR=$dir"
    Write-Output "FILE_COUNT=$($files.Count)"
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($dir.TrimEnd('\').Length + 1)
        Write-Output "$relative`t$($file.Length)`t$((Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash)"
    }
}

Write-Manifest 'AozoraFavorites Ultimate Edition' $coreDir
Write-Manifest 'AozoraFavorites Ultimate Edition CHS Patch' $chsDir
