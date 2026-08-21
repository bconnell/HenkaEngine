Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$findings = New-Object System.Collections.Generic.List[string]

function Add-Finding {
    param([string]$Message)
    $findings.Add($Message)
}

function Get-RepositoryText {
    param([string]$RelativePath)

    $path = Join-Path $repoRoot ($RelativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-Finding "${RelativePath}: file is missing"
        return ""
    }
    return [System.IO.File]::ReadAllText($path)
}

$readme = Get-RepositoryText "README.md"
$capabilities = Get-RepositoryText "docs/current-capabilities.md"
$roadmap = Get-RepositoryText "docs/roadmap.md"
$scripting = Get-RepositoryText "docs/scripting-foundation.md"

foreach ($stalePattern in @(
    "(?i)modeling follows the production-quality 2\.5D track",
    "(?i)then integrated modeling",
    "(?i)vertex/edge deletion.{0,100}remain in progress",
    "(?i)full host API coverage.{0,100}`for` loops.{0,100}future"
)) {
    foreach ($document in @(
        [pscustomobject]@{ Path = "README.md"; Text = $readme },
        [pscustomobject]@{ Path = "docs/current-capabilities.md"; Text = $capabilities },
        [pscustomobject]@{ Path = "docs/roadmap.md"; Text = $roadmap },
        [pscustomobject]@{ Path = "docs/scripting-foundation.md"; Text = $scripting }
    )) {
        if ($document.Text -match $stalePattern) {
            Add-Finding "$($document.Path): stale capability wording matches '$stalePattern'"
        }
    }
}

foreach ($operation in @(
    "Merge Center",
    "Merge Active",
    "Merge by Distance",
    "Connect Vertices",
    "Dissolve Vertex",
    "Delete Vertex",
    "Vertex Bevel"
)) {
    if ($capabilities -notmatch [regex]::Escape($operation)) {
        Add-Finding "docs/current-capabilities.md: missing published Vertex operation '$operation'"
    }
}
if ($capabilities -notmatch "(?i)Vertex Extrude") {
    Add-Finding "docs/current-capabilities.md: Vertex Extrude must remain explicitly listed as unfinished"
}
if ($capabilities -notmatch "(?i)Edge Delete") {
    Add-Finding "docs/current-capabilities.md: Edge Delete status must remain explicit"
}
if ($roadmap -notmatch "(?i)Integrated authoring is already underway" -or
    $roadmap -notmatch "(?im)^### Implemented Foundation" -or
    $roadmap -notmatch "(?im)^### Current Development" -or
    $roadmap -notmatch "(?im)^### Future Work") {
    Add-Finding "docs/roadmap.md: integrated authoring must distinguish foundation, current development, and future work"
}

if ($readme -notmatch "(?im)^>\s*\*\*Support Henka Engine\*\*" -or
    $readme -notmatch "(?im)^##\s+Support Henka Engine" -or
    $readme -notmatch "(?i)SUPPORT\.md" -or
    $readme -notmatch "(?i)https://github\.com/sponsors/") {
    Add-Finding "README.md: sponsorship callout, dedicated section, SUPPORT.md, and direct Sponsors link are all required"
}

if ($scripting -notmatch "(?i)Input\.IsActionDown.{0,180}fail-closed" -or
    $scripting -notmatch "(?i)Interaction\.Try.{0,180}unavailable" -or
    $scripting -notmatch "(?i)entity_is_valid\(entity\)" -or
    $scripting -notmatch "(?i)transform_get_position\(entity\)" -or
    $scripting -notmatch "(?i)physics_apply_impulse\(entity, impulse\)") {
    Add-Finding "docs/scripting-foundation.md: current script host availability and HenkaScript typed gameplay surface must be explicit"
}

$trackedMarkdown = @(& (Get-HenkaGitPath) -C $repoRoot ls-files --cached --others --exclude-standard "*.md")
if ($LASTEXITCODE -ne 0) {
    throw "Unable to enumerate Markdown files."
}
foreach ($rawPath in $trackedMarkdown) {
    $relativePath = ([string]$rawPath).Trim().Replace("\", "/")
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
        continue
    }
    $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        continue
    }
    $text = [System.IO.File]::ReadAllText($absolutePath)
    foreach ($match in [regex]::Matches($text, '\[[^\]]+\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim()
        if ([string]::IsNullOrWhiteSpace($target) -or
            $target.StartsWith("#") -or
            $target -match '^[A-Za-z][A-Za-z0-9+.-]*://') {
            continue
        }
        $localTarget = ($target -split '#', 2)[0]
        if ([string]::IsNullOrWhiteSpace($localTarget)) {
            continue
        }
        $resolvedTarget = Join-Path (Split-Path -Parent $absolutePath) $localTarget.Replace("/", "\")
        if (-not (Test-Path -LiteralPath $resolvedTarget)) {
            Add-Finding "${relativePath}: broken local Markdown link '$target'"
        }
    }
}

if ($findings.Count -gt 0) {
    Write-Host "Documentation truth check failed:"
    $findings | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[pass] Documentation truth and local-link check passed."
