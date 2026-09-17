[CmdletBinding()]
param(
    [string]$FlexRoot
)

if ([string]::IsNullOrWhiteSpace($FlexRoot)) {
    $FlexRoot = Join-Path $PSScriptRoot 'vendor\flex-sdk'
}

$vendorRoot = Split-Path -Parent $FlexRoot
$sdkZip = Join-Path $vendorRoot 'apache-flex-sdk-4.16.1-bin.zip'
$sdkUrl = 'https://dlcdn.apache.org/flex/4.16.1/binaries/apache-flex-sdk-4.16.1-bin.zip'
$playerRoot = Join-Path $FlexRoot 'frameworks\libs\player'
$playerDir = Join-Path $playerRoot '11.2'
$playerSwc = Join-Path $playerDir 'playerglobal.swc'
$playerUrl = 'https://raw.githubusercontent.com/nexussays/playerglobal/master/11.2/playerglobal.swc'

New-Item -ItemType Directory -Force -Path $vendorRoot | Out-Null
if (-not (Test-Path -LiteralPath $sdkZip)) {
    Invoke-WebRequest -Uri $sdkUrl -OutFile $sdkZip
}
if (-not (Test-Path -LiteralPath (Join-Path $FlexRoot 'bin\mxmlc.bat'))) {
    Expand-Archive -LiteralPath $sdkZip -DestinationPath $FlexRoot -Force
}
New-Item -ItemType Directory -Force -Path $playerDir | Out-Null
if (-not (Test-Path -LiteralPath $playerSwc)) {
    Invoke-WebRequest -Uri $playerUrl -OutFile $playerSwc
}

Write-Output "FLEX_SDK=$FlexRoot"
Write-Output "MXMLC=$(Join-Path $FlexRoot 'bin\mxmlc.bat')"
Write-Output "PLAYERGLOBAL=$playerSwc"
