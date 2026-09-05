param()

$ErrorActionPreference = "Stop"
$NativeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UiSourcePath = Join-Path $NativeDir "overlay_layered.cpp"
$TablePath = Join-Path $NativeDir "localization.cpp"

$uiSource = Get-Content -Raw -LiteralPath $UiSourcePath
$tableSource = Get-Content -Raw -LiteralPath $TablePath
$regexOptions = [System.Text.RegularExpressions.RegexOptions]::Singleline
$trPattern = [regex]::new(
    'Tr\s*\(\s*L"((?:\\.|[^"\\])*)"\s*\)',
    $regexOptions)
$definitionPattern = [regex]::new(
    '\{\s*L"((?:\\.|[^"\\])*)"\s*,\s*L"',
    $regexOptions)
$fullDefinitionPattern = [regex]::new(
    '\{\s*L"(?:\\.|[^"\\])*"\s*,\s*' +
    'L"(?:\\.|[^"\\])*"\s*,\s*' +
    'L"(?:\\.|[^"\\])*"\s*,\s*' +
    'L"(?:\\.|[^"\\])*"\s*,\s*' +
    'L"(?:\\.|[^"\\])*"\s*\}',
    $regexOptions)

$used = @($trPattern.Matches($uiSource) | ForEach-Object {
    $_.Groups[1].Value
} | Sort-Object -Unique)
$defined = @($definitionPattern.Matches($tableSource) | ForEach-Object {
    $_.Groups[1].Value
})
$definedUnique = @($defined | Sort-Object -Unique)
$missing = @($used | Where-Object { $_ -notin $definedUnique })
$duplicates = @($defined | Group-Object | Where-Object Count -gt 1)
$completeDefinitions = @($fullDefinitionPattern.Matches($tableSource))

if ($missing.Count -gt 0) {
    Write-Error ("Missing localization entries:`n" + ($missing -join "`n"))
}
if ($duplicates.Count -gt 0) {
    Write-Error ("Duplicate localization entries:`n" +
        (($duplicates | ForEach-Object Name) -join "`n"))
}
if ($completeDefinitions.Count -ne $defined.Count) {
    Write-Error (
        "Malformed localization table: expected $($defined.Count) complete five-language rows, " +
        "found $($completeDefinitions.Count).")
}

# Remove translated calls, disabled legacy blocks, and comments before looking
# for raw Han literals. This catches a newly drawn Chinese label or prompt even
# when the developer forgot to add Tr(...), which a table-only audit cannot.
$runtimeSource = $trPattern.Replace($uiSource, '')
$runtimeSource = [regex]::Replace(
    $runtimeSource, '#if\s+0.*?#endif', '', $regexOptions)
$runtimeSource = [regex]::Replace(
    $runtimeSource, '/\*.*?\*/', '', $regexOptions)
$runtimeSource = [regex]::Replace(
    $runtimeSource, '//.*?$', '',
    [System.Text.RegularExpressions.RegexOptions]::Multiline)
$wideLiteralPattern = [regex]::new('L"((?:\\.|[^"\\])*)"')
$rawHan = @($wideLiteralPattern.Matches($runtimeSource) | Where-Object {
    $_.Groups[1].Value -match '[\p{IsCJKUnifiedIdeographs}：；，。（）【】“”]'
} | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
if ($rawHan.Count -gt 0) {
    Write-Error ("Untranslated runtime UI literals:`n" + ($rawHan -join "`n"))
}

Write-Host "Localization source audit passed: $($used.Count) used, $($definedUnique.Count) defined, 0 missing."
exit 0
