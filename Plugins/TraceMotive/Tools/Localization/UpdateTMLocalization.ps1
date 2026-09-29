param(
    [string]$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$ProjectPath = (Resolve-Path (Join-Path $PluginRoot '..\..\ToyBuilderC.uproject')).Path,
    [string]$UnrealEditorCmd = '',
    [switch]$SkipGather
)

$ErrorActionPreference = 'Stop'
$ConfigPath = Join-Path $PluginRoot 'Config\Localization\TraceMotive.ini'
$GenerateKeys = Join-Path $PluginRoot 'Tools\Localization\GenerateTMLocTextKeys.py'
$Validate = Join-Path $PluginRoot 'Tools\Localization\ValidateTMLocalization.ps1'

& python $GenerateKeys
if ($LASTEXITCODE -ne 0) { throw 'TMLoc key generation failed.' }
& powershell -ExecutionPolicy Bypass -File $Validate -PluginRoot $PluginRoot
if ($LASTEXITCODE -ne 0) { throw 'Localization validation failed.' }

if (!$SkipGather) {
    if ([string]::IsNullOrWhiteSpace($UnrealEditorCmd)) {
        throw 'Pass -UnrealEditorCmd with the UnrealEditor-Cmd.exe path for the engine version you want to gather with.'
    }
    if (!(Test-Path $UnrealEditorCmd)) { throw "UnrealEditor-Cmd not found: $UnrealEditorCmd" }
    $ProjectRoot = Split-Path $ProjectPath -Parent
    Push-Location $ProjectRoot
    try {
        & $UnrealEditorCmd $ProjectPath -run=GatherText -config='Plugins/TraceMotive/Config/Localization/TraceMotive.ini' -unattended -nop4 -NullRHI -NoSplash
        if ($LASTEXITCODE -ne 0) { throw "GatherText failed: $LASTEXITCODE" }
    }
    finally {
        Pop-Location
    }
}

Write-Host '[TraceMotive Localization] Done. Edit runtime Korean text in TMLocalization.ko.overrides.csv. Use the UE Localization Dashboard or ko PO file only for locres-based text.'
