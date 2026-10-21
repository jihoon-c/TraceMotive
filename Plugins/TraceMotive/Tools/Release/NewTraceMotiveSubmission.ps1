param(
    [Parameter(Mandatory = $true)][string]$BuiltPluginRoot,
    [Parameter(Mandatory = $true)][string]$DestinationRoot,
    [string]$ArtifactName = 'TraceMotive_1.0.0_UE5.8.3_Source'
)

$ErrorActionPreference = 'Stop'
$builtRoot = (Resolve-Path -LiteralPath $BuiltPluginRoot).Path
$artifactRoot = [IO.Path]::GetFullPath($DestinationRoot)
if ($ArtifactName -notmatch '^[A-Za-z0-9_.-]+$') { throw 'ArtifactName must be a simple file name.' }
$stageRoot = Join-Path $artifactRoot $ArtifactName
$pluginStage = Join-Path $stageRoot 'TraceMotive'
$zipPath = Join-Path $artifactRoot ($ArtifactName + '.zip')
if ((Test-Path -LiteralPath $stageRoot) -or (Test-Path -LiteralPath $zipPath)) {
    throw 'Use a new output path; existing release artifacts are never overwritten.'
}
$required = @('TraceMotive.uplugin', 'Source', 'Resources', 'Config', 'Content/Localization',
    'README.md', 'USER_GUIDE.md', 'PRIVACY.md', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md')
if (Test-Path -LiteralPath (Join-Path $builtRoot 'README_KO.md')) { $required += 'README_KO.md' }
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $builtRoot $relative))) { throw "Missing packaged input: $relative" }
}
$descriptor = Get-Content -LiteralPath (Join-Path $builtRoot 'TraceMotive.uplugin') -Raw | ConvertFrom-Json
if (@($descriptor.Modules).Count -eq 0 -or @($descriptor.Modules | Where-Object Type -ne 'Editor').Count -gt 0) {
    throw 'Expected an editor-only plugin with at least one module.'
}
New-Item -ItemType Directory -Path $pluginStage -Force | Out-Null
foreach ($relative in $required) {
    $destination = Join-Path $pluginStage $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $builtRoot $relative) -Destination $destination -Recurse
}
$files = @(Get-ChildItem -LiteralPath $pluginStage -Recurse -File)
$manifest = @($files | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($stageRoot, $_.FullName).Replace('\', '/')
    if ($relative -match '(^|/)(Binaries|Intermediate|Saved|DerivedDataCache|\.git)(/|$)' -or
        $_.Extension -in @('.dll', '.pdb', '.lib', '.obj', '.log', '.uasset', '.umap')) {
        throw "Unexpected release file: $relative"
    }
    [PSCustomObject]@{ Path = $relative; Bytes = $_.Length; SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
$stylePath = Join-Path $pluginStage 'Source/TraceMotive/Private/TMStyle.cpp'
$style = Get-Content -LiteralPath $stylePath -Raw
$icons = [regex]::Matches($style, 'AddVectorIcon\(TEXT\("[^\"]+"\), TEXT\("Icons/([^\"]+)"\)')
foreach ($icon in $icons) {
    $relative = 'Resources/Icons/Unified/' + $icon.Groups[1].Value + '.svg'
    if (-not (Test-Path -LiteralPath (Join-Path $pluginStage $relative))) { throw "Missing style resource: $relative" }
}
Compress-Archive -LiteralPath $pluginStage -DestinationPath $zipPath -CompressionLevel Optimal
$archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entries = @($archive.Entries | Where-Object { $_.Name })
    if ($entries.Count -ne $files.Count) { throw 'ZIP entry count differs from the staged files.' }
    foreach ($entry in $entries) {
        if (-not $entry.FullName.Replace('\', '/').StartsWith('TraceMotive/') -or $entry.FullName -match '(^|[/\\])\.\.([/\\]|$)') {
            throw "Invalid ZIP entry: $($entry.FullName)"
        }
    }
} finally { $archive.Dispose() }
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $artifactRoot ($ArtifactName + '.manifest.json')) -Encoding utf8
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
"$zipHash  $ArtifactName.zip" | Set-Content -LiteralPath ($zipPath + '.sha256') -Encoding ascii
Write-Output "Created $zipPath ($($files.Count) files, $($icons.Count) style icons checked)"
Write-Output "SHA256=$zipHash"
