param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory,

    [Alias("RequireNativeAuthored")]
    [switch]$RequireEditorDerivedFixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Geometry Solid evidence directory was not found: $InputDirectory"
}
$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "Geometry Solid evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: GEOMETRY_SOLID\s*$") {
    throw "Geometry Solid evidence does not declare the GEOMETRY_SOLID profile."
}
if ($indexText -notmatch "Source: .*henka_sandbox3d\.exe") {
    throw "Geometry Solid evidence does not identify the Henka Sandbox executable."
}

$requiredFiles = @(
    "startup-showcase.png",
    "same-camera-solid.png",
    "giraffe-front-solid.png",
    "giraffe-three-quarter-solid.png",
    "giraffe-profile-solid.png",
    "rocket-front-solid.png",
    "rocket-three-quarter-solid.png",
    "rocket-profile-solid.png"
)
Add-Type -AssemblyName System.Drawing
foreach ($file in $requiredFiles) {
    $path = Join-Path $InputDirectory $file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Geometry Solid evidence is missing $file."
    }
    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "Geometry Solid evidence has invalid dimensions for $file."
        }
        [double]$minimum = 255.0
        [double]$maximum = 0.0
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $minimum = [Math]::Min($minimum, $luma)
                $maximum = [Math]::Max($maximum, $luma)
                $sum += $luma
                $sumSquares += $luma * $luma
                ++$count
            }
        }
        [double]$mean = $sum / $count
        [double]$variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        if ([Math]::Sqrt($variance) -lt 1.0 -or ($maximum - $minimum) -lt 8.0) {
            throw "Geometry Solid evidence is too flat for $file."
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$metadata = [regex]::Matches(
    $indexText,
    '(?m)^\w[^\r\n]*metadata: CAPTURE_READY mode=(?<mode>[a-z_]+)[^\r\n]*giraffe_provenance=(?<giraffe>[A-Z_]+) rocket_provenance=(?<rocket>[A-Z_]+) preset_applied=(?<preset>[01]) capture_subject=(?<subject>pair|giraffe|rocket) draw_expected=1')
if ($metadata.Count -lt 8) {
    throw "Geometry Solid evidence is missing application readiness metadata for all required views."
}
$subjectCounts = @{}
$editorDerivedFixture = $true
foreach ($record in $metadata) {
    $subject = $record.Groups["subject"].Value
    if (-not $subjectCounts.ContainsKey($subject)) {
        $subjectCounts[$subject] = 0
    }
    ++$subjectCounts[$subject]
    if ($record.Groups["preset"].Value -ne "0") {
        throw "Geometry Solid evidence was produced after an asset-specific preset."
    }
    if ($record.Groups["giraffe"].Value -ne "HENKA_NATIVE_EDITED_FIXTURE" -or
        $record.Groups["rocket"].Value -ne "HENKA_NATIVE_EDITED_FIXTURE") {
        $editorDerivedFixture = $false
    }
}
foreach ($requiredSubject in @("pair", "giraffe", "rocket")) {
    $minimumCount = if ($requiredSubject -eq "pair") { 2 } else { 3 }
    if (-not $subjectCounts.ContainsKey($requiredSubject) -or $subjectCounts[$requiredSubject] -lt $minimumCount) {
        throw "Geometry Solid evidence is missing the required $requiredSubject capture set."
    }
}
if ($RequireEditorDerivedFixture -and -not $editorDerivedFixture) {
    throw "Geometry Solid evidence is missing the expected editor-derived fixture state."
}

$status = if ($editorDerivedFixture) { "HENKA_NATIVE_EDITED_FIXTURE state present; independent user-authored provenance remains unverified" } else { "fixture-only evidence; editor-derived fixture state remains unverified" }
@(
    "Geometry Solid visual evidence validation: passed",
    "Application-only source: Henka Sandbox executable identified in INDEX.txt",
    "Neutral Solid views: startup, pair, Giraffe front/three-quarter/profile, Rocket front/three-quarter/profile",
    "Geometry authority: $status",
    "Textures, normals, roughness, subsurface, fog, and post effects are not accepted as geometry credit",
    "Status: structural evidence guard passed; human geometry QA remains required"
) | Set-Content -LiteralPath (Join-Path $InputDirectory "geometry-solid-validation.txt")
Write-Output "[pass] Geometry Solid evidence validated ($status)"
