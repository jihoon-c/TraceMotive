param(
    [string]$PluginRoot = (Resolve-Path "$PSScriptRoot\..\..").Path
)

$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path $PluginRoot 'Source\TraceMotive'
$generatedKeysPath = Join-Path $sourceRoot 'Private\TMLocTextKeys.cpp'
$sourceFiles = Get-ChildItem -LiteralPath $sourceRoot -Include *.cpp,*.h -Recurse -File |
    Where-Object { $_.FullName -ne $generatedKeysPath }
$sourceText = ($sourceFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"

$rawStaticText = [regex]::Matches($sourceText, 'FText::FromString\(TEXT\("')
$tmLocCalls = [regex]::Matches($sourceText, 'TMLoc::(?:Text|String)\(\s*TEXT\("')
$corruptionPattern = [regex]::new([regex]::Escape([string][char]0xFFFD))
$corruption = $corruptionPattern.Matches($sourceText)
$hasLegacyProductMojibake = $sourceText.Contains('TraceMotive ' + '??' + 'Debug')
$errors = @()

if ($tmLocCalls.Count -eq 0) { $errors += 'No TMLoc calls were found.' }
if ($rawStaticText.Count -gt 0) { $errors += "Static UI text bypasses Localization: $($rawStaticText.Count) occurrence(s)." }
if ($corruption.Count -gt 0 -or $hasLegacyProductMojibake) { $errors += "Corrupted-looking source text found." }
if (-not (Test-Path -LiteralPath $generatedKeysPath)) { $errors += 'Generated TMLocTextKeys.cpp is missing.' }

$translations = @{}
foreach ($csvName in 'TMLocalization.ko.csv', 'TMLocalization.ko.overrides.csv') {
    $csvPath = Join-Path $PluginRoot "Config\Localization\$csvName"
    if (-not (Test-Path -LiteralPath $csvPath)) {
        $errors += "Required runtime translation file is missing: $csvName"
        continue
    }
    foreach ($row in Import-Csv -LiteralPath $csvPath -Encoding UTF8) {
        if (-not [string]::IsNullOrWhiteSpace($row.Key) -and -not [string]::IsNullOrWhiteSpace($row.ko)) {
            $translations[$row.Key] = $row.ko
        }
    }
}

if (Test-Path -LiteralPath $generatedKeysPath) {
    $generatedText = Get-Content -LiteralPath $generatedKeysPath -Raw -Encoding UTF8
    $generatedKeys = [regex]::Matches($generatedText, 'NSLOCTEXT\("TraceMotive", "((?:\\.|[^"\\])*)"') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    $neutralKeys = @('-', '->', '!', '...', '*', '\\u00D7', '\\u2192', '\\u25B6', '\\u25CB', '\\u25CF', '\\u2713', '>', '|', '00:00', 'A', 'B', 'EV', 'S', 'UI')
    $missingKeys = @($generatedKeys | Where-Object {
        $_ -notin $neutralKeys -and
        (-not $translations.ContainsKey($_) -or $translations[$_] -eq $_)
    })
    if ($missingKeys.Count -gt 0) {
        $errors += "Missing Korean runtime translations: $($missingKeys.Count) key(s): $($missingKeys -join ' | ')"
    }
}

$guideKeys = [regex]::Matches($sourceText, 'GuideText\(TEXT\("((?:\\.|[^"\\])*)"\)\)') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$missingGuideKeys = @($guideKeys | Where-Object {
    -not $translations.ContainsKey($_) -or $translations[$_] -eq $_
})
if ($missingGuideKeys.Count -gt 0) {
    $errors += "Missing Korean guide translations: $($missingGuideKeys.Count) key(s): $($missingGuideKeys -join ' | ')"
}

foreach ($culture in 'en', 'ko') {
    $poPath = Join-Path $PluginRoot "Content\Localization\TraceMotive\$culture\TraceMotive.po"
    if (Test-Path -LiteralPath $poPath) {
        $poText = Get-Content -LiteralPath $poPath -Raw -Encoding UTF8
        if ($corruptionPattern.IsMatch($poText)) { $errors += "Corrupted-looking translation found in $culture PO." }
    }
}

if ($errors.Count -gt 0) {
    $errors | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    exit 1
}

Write-Host "Localization validation passed. TMLocCalls=$($tmLocCalls.Count) RuntimeTranslations=$($translations.Count) GuideKeys=$($guideKeys.Count)" -ForegroundColor Green
