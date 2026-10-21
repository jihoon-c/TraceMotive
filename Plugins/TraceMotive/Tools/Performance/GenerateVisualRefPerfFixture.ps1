param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,
    [int]$SmallFileCount = 512,
    [int]$LargeFileCount = 16
)

$resolvedProject = [IO.Path]::GetFullPath($ProjectRoot)
$sourceRoot = Join-Path $resolvedProject 'Source'
$fixtureRoot = Join-Path $sourceRoot 'TraceMotivePerfFixture'
if (-not (Test-Path -LiteralPath $resolvedProject -PathType Container)) {
    throw "Project directory does not exist: $resolvedProject"
}
if (@(Get-ChildItem -LiteralPath $resolvedProject -Filter '*.uproject' -File).Count -eq 0) {
    throw "No .uproject file at project root: $resolvedProject"
}
if (Test-Path -LiteralPath $fixtureRoot) {
    throw "Fixture directory already exists: $fixtureRoot"
}
if ($SmallFileCount -lt 0 -or $LargeFileCount -lt 0 -or ($SmallFileCount + $LargeFileCount) -eq 0) {
    throw 'File counts must be nonnegative and their sum must be positive.'
}

New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
$encoding = [Text.UTF8Encoding]::new($false)
function Write-FixtureFile([string]$path, [int]$targetBytes, [int]$index) {
    $builder = [Text.StringBuilder]::new()
    [void]$builder.AppendLine("int32 TMPerfValue = $index;")
    $line = "const int32 FixtureCounter = 12345; // representative source text for VisualRef scan`n"
    while ($builder.Length -lt $targetBytes) { [void]$builder.Append($line) }
    [IO.File]::WriteAllText($path, $builder.ToString(), $encoding)
}
for ($index = 0; $index -lt $SmallFileCount; ++$index) {
    Write-FixtureFile (Join-Path $fixtureRoot ('Small_{0:D4}.cpp' -f $index)) 8192 $index
}
for ($index = 0; $index -lt $LargeFileCount; ++$index) {
    Write-FixtureFile (Join-Path $fixtureRoot ('Large_{0:D4}.cpp' -f $index)) 262144 $index
}
$files = @(Get-ChildItem -LiteralPath $fixtureRoot -File)
$totalBytes = ($files | Measure-Object -Property Length -Sum).Sum
Write-Output "Generated $($files.Count) source files ($totalBytes bytes) at $fixtureRoot"
