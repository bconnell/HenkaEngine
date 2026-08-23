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
$authoringMesh = Get-RepositoryText "docs/authoring-mesh.md"
$componentIdentities = Get-RepositoryText "docs/authoring-component-identities.md"
$authoringMeshSource = Get-RepositoryText "engine/src/mesh/authoring_mesh.c"
$scripting = Get-RepositoryText "docs/scripting-foundation.md"
$externalProjects = Get-RepositoryText "docs/external-game-projects.md"
$externalTemplate = Get-RepositoryText "templates/external_game_minimal/src/main.c"
$sandboxMain = Get-RepositoryText "examples/sandbox3d/main.c"
$sandboxHelp = Get-RepositoryText "docs/help/sandbox3d.md"
$sandboxQa = Get-RepositoryText "docs/qa/sandbox3d-manual-checklist.md"
$showcaseAssets = Get-RepositoryText "docs/showcase-assets.md"

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

$hamsVersionMatch = [regex]::Match(
    $authoringMeshSource,
    '(?m)^\s*#define\s+HENKA_AUTHORING_MESH_FILE_VERSION\s+(\d+)U\b')
if (-not $hamsVersionMatch.Success) {
    Add-Finding "engine/src/mesh/authoring_mesh.c: current HAMS writer version could not be determined"
} else {
    $currentHamsVersion = [int]$hamsVersionMatch.Groups[1].Value
    $currentHamsClaim = "HAMS v$currentHamsVersion"
    foreach ($document in @(
        [pscustomobject]@{ Path = "docs/authoring-mesh.md"; Text = $authoringMesh },
        [pscustomobject]@{ Path = "docs/current-capabilities.md"; Text = $capabilities },
        [pscustomobject]@{ Path = "docs/authoring-component-identities.md"; Text = $componentIdentities }
    )) {
        if ($document.Text -notmatch [regex]::Escape($currentHamsClaim)) {
            Add-Finding "$($document.Path): current HAMS writer version '$currentHamsClaim' is not documented"
        }
        foreach ($staleCurrentClaim in @(
            "(?i)\bwrites\s+HAMS\s+v(?!$currentHamsVersion\b)\d+",
            "(?i)\bcurrent\s+v(?!$currentHamsVersion\b)\d+\s+format",
            "(?i)\bcurrent\s+HAMS\s+v(?!$currentHamsVersion\b)\d+",
            "(?i)\bcurrent\s+writer\b.{0,40}\bHAMS\s+v(?!$currentHamsVersion\b)\d+"
        )) {
            if ($document.Text -match $staleCurrentClaim) {
                Add-Finding "$($document.Path): stale current HAMS claim matches '$staleCurrentClaim'"
            }
        }
    }
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

if ($externalProjects -notmatch '(?i)package-owned `\.hks` and `\.lua` assets' -or
    $externalProjects -notmatch "(?i)HKS-to-Lua" -or
    $externalTemplate -notmatch "assets/scripts/publisher\.hks" -or
    $externalTemplate -notmatch "assets/scripts/subscriber\.lua" -or
    $externalTemplate -notmatch "HENKA_SCRIPT_LANGUAGE_HENKASCRIPT" -or
    $externalTemplate -notmatch "HENKA_SCRIPT_LANGUAGE_LUA") {
    Add-Finding "external game template: public mixed-language package proof must remain source-visible"
}

if ($sandboxMain -match '"Hidden:"') {
    Add-Finding "examples/sandbox3d/main.c: hidden-object rows must use the explicit Visibility: Hidden label"
}

foreach ($document in @(
    [pscustomobject]@{ Path = "docs/help/sandbox3d.md"; Text = $sandboxHelp },
    [pscustomobject]@{ Path = "docs/qa/sandbox3d-manual-checklist.md"; Text = $sandboxQa },
    [pscustomobject]@{ Path = "docs/showcase-assets.md"; Text = $showcaseAssets }
)) {
    if ($document.Text -match '(?i)Create Native Rocket') {
        Add-Finding "$($document.Path): removed asset-specific Create Native Rocket action is still documented"
    }
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
