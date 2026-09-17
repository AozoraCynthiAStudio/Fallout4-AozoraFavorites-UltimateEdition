[CmdletBinding()]
param(
    [string]$Version = 'v1n',
    [string]$NativeDll = (Join-Path $PSScriptRoot '..\src\native\build\windows\x64\release\AozoraFavoritesSWF.dll'),
    [string]$Swf = (Join-Path $PSScriptRoot '..\build\AozoraFavoritesMenu.swf'),
    [string]$PluginEsp = (Join-Path $PSScriptRoot '..\..\..\Data\AozoraFavorites.esp'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\package\AozoraFavorites_skyui_like_v1n')
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$NativeDll = (Resolve-Path -LiteralPath $NativeDll).Path
$Swf = (Resolve-Path -LiteralPath $Swf).Path
$PluginEsp = (Resolve-Path -LiteralPath $PluginEsp).Path
$textureRoot = Join-Path $root 'assets\runtime\Textures\Interface\AozoraFavorites'
$mcmRoot = Join-Path $root 'mcm'
$legacyScripts = Join-Path $root '..\..\release-2.1\AozoraFavorites_2.1.0_CHS_LegacyInputPath_20260908\Scripts'
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

if (Test-Path -LiteralPath $OutputDir) {
    throw "Refusing to overwrite existing package directory: $OutputDir"
}
$textures = @(Get-ChildItem -LiteralPath $textureRoot -File -Filter '*.dds')
if ($textures.Count -ne 53) { throw "Expected 53 runtime textures, found $($textures.Count)" }

$dirs = @(
    'F4SE\Plugins', 'Interface', 'Textures\Interface\AozoraFavorites',
    'Scripts', 'MCM\Config\AozoraFavorites', 'MCM\Settings'
)
foreach ($dir in $dirs) { New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir $dir) | Out-Null }

Copy-Item -LiteralPath $NativeDll -Destination (Join-Path $OutputDir 'F4SE\Plugins\AozoraFavoritesSWF.dll')
Copy-Item -LiteralPath $Swf -Destination (Join-Path $OutputDir 'Interface\AozoraFavoritesMenu.swf')
Copy-Item -LiteralPath $PluginEsp -Destination (Join-Path $OutputDir 'AozoraFavorites.esp')
foreach ($texture in $textures) {
    Copy-Item -LiteralPath $texture.FullName -Destination (Join-Path $OutputDir 'Textures\Interface\AozoraFavorites')
}
foreach ($scriptName in @('AozoraMenuNative.pex', 'AozoraMenuRun.pex')) {
    $script = Join-Path $legacyScripts $scriptName
    if (Test-Path -LiteralPath $script) { Copy-Item -LiteralPath $script -Destination (Join-Path $OutputDir 'Scripts') }
}
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Config\AozoraFavorites\config.json') -Destination (Join-Path $OutputDir 'MCM\Config\AozoraFavorites\config.json')
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Config\AozoraFavorites\keybinds.json') -Destination (Join-Path $OutputDir 'MCM\Config\AozoraFavorites\keybinds.json')
Copy-Item -LiteralPath (Join-Path $mcmRoot 'MCM\Settings\AozoraFavorites.ini') -Destination (Join-Path $OutputDir 'MCM\Settings\AozoraFavorites.ini')

$readme = @(
    ('Aozora Favorites skyui like ' + $Version),
    '',
    ('This is the ' + $Version + ' package for the skyui like baseline.'),
    'The panel is drawn by AS3 at runtime. It does not include the retired device-shell skins.',
    'Theme mode 0 follows the game gameplay HUD color. Theme mode 1 uses the six preset palettes.',
    'The amber selection state and mascot artwork remain independent from the theme tint.',
    'FIS icons require the separately installed FallUI - Icon Library (Interface/FallUI_IconLib.swf); that third-party asset is not redistributed here.',
    'Aozora Store is the only persistent favorite source. Pip-Boy Q toggles collection; Pip-Boy number keys 1~= are intentionally disabled and only show a hint. Aozora hotkeys remain independent and are edited only in the Aozora Favorites menu.',
    'Layout tuning is in MCM/Settings/AozoraFavorites.ini under the SkyuiLike16x9 baseline; runtime aspect-ratio and resolution adaptation is automatic.',
    '',
    'Static checks performed: AS3 SWF build, Native DLL build, 53 texture inventory, and package file copy.',
    'In-game validation is still required: open/close, keyboard/controller input, color modes, layout editor, long names, empty list, scrolling, mascot and silhouette visibility.',
    'Install only this Aozora test package in MO2 while testing.'
)
[IO.File]::WriteAllText((Join-Path $OutputDir ('README_skyui_like_' + $Version + '.txt')), ($readme -join [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $OutputDir 'VERSION.txt'), $Version, [Text.UTF8Encoding]::new($false))

$files = Get-ChildItem -LiteralPath $OutputDir -File -Recurse
Write-Output "DIR=$OutputDir"
Write-Output "FILE_COUNT=$($files.Count)"
foreach ($file in $files) {
    $relative = $file.FullName.Substring($OutputDir.TrimEnd('\').Length + 1)
    Write-Output "$relative`t$($file.Length)`t$((Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash)"
}
