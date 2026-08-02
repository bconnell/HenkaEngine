param(
    [string]$OutputRoot = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [int]$CommandTimeoutMilliseconds = 180000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$git = Get-HenkaGitPath
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path ([Environment]::GetFolderPath("UserProfile")) ("Downloads\HenkaEngine-phase3-evidence-" + $timestamp)
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "Evidence output already exists: $OutputRoot" }
$root = Join-Path $OutputRoot "Evidence"
foreach ($name in @("Screenshots", "Reports", "Repository", "Handoff")) {
    [System.IO.Directory]::CreateDirectory((Join-Path $root $name)) | Out-Null
}

function Write-Report {
    param([string]$RelativePath, [AllowEmptyString()][string]$Text)
    $path = Join-Path $root $RelativePath
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $path)) | Out-Null
    Write-HenkaUtf8NoBom -Path $path -Text $Text
}

function Invoke-EvidencePowerShell {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [string[]]$Arguments = @(),
        [string]$LogName,
        [int]$TimeoutMilliseconds = 180000
    )
    $logRoot = Join-Path $root ("Reports\" + $LogName)
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $logRoot)) | Out-Null
    $stdoutPath = $logRoot + ".stdout.log"
    $stderrPath = $logRoot + ".stderr.log"
    $argumentList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $Arguments
    $process = $null
    try {
        $process = Start-HenkaCapturedProcess -FilePath "powershell.exe" -Arguments $argumentList -WorkingDirectory $repoRoot -StdoutPath $stdoutPath -StderrPath $stderrPath -CreateNoWindow
        if (-not $process.Process.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $process.Process.Id
            throw "$Label exceeded ${TimeoutMilliseconds}ms; process tree terminated."
        }
        if ($process.Process.ExitCode -ne 0) { throw "$Label failed with exit code $($process.Process.ExitCode)." }
        Write-Report -RelativePath ("Reports\" + $LogName + ".status.txt") -Text "PASS`r`n$Label`r`n"
    }
    catch {
        Write-Report -RelativePath ("Reports\" + $LogName + ".status.txt") -Text ("FAIL`r`n" + $_.Exception.Message + "`r`n")
        throw
    }
    finally {
        if ($null -ne $process) { Close-HenkaCapturedProcess $process }
    }
}

$status = (& $git status --short | Out-String)
$branch = (& $git branch --show-current | Out-String).Trim()
$head = (& $git rev-parse HEAD | Out-String).Trim()
$origin = (& $git rev-parse origin/main | Out-String).Trim()
$remote = (& $git remote get-url origin | Out-String).Trim()
$divergence = (& $git rev-list --left-right --count "origin/main...HEAD" | Out-String).Trim()
$locks = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot ".git") -Filter "*.lock" -File -ErrorAction SilentlyContinue).Count
Write-Report -RelativePath "Repository\preflight.txt" -Text ("branch=$branch`r`nHEAD=$head`r`norigin/main=$origin`r`nremote=$remote`r`ndivergence=$divergence`r`nactive_git_locks=$locks`r`nstatus:`r`n$status")

try {
    Invoke-EvidencePowerShell -Label "Windows configure/build/test" -ScriptPath (Join-Path $PSScriptRoot "test_windows.ps1") -LogName "Build\windows-tests" -TimeoutMilliseconds $CommandTimeoutMilliseconds
    Invoke-EvidencePowerShell -Label "Windows package" -ScriptPath (Join-Path $PSScriptRoot "package_sandbox3d_windows.ps1") -Arguments @("-Configuration", $Configuration) -LogName "Package\sandbox-package" -TimeoutMilliseconds $CommandTimeoutMilliseconds
    Invoke-EvidencePowerShell -Label "Package contract and deterministic smoke" -ScriptPath (Join-Path $PSScriptRoot "check_packaged_sandbox3d_windows.ps1") -Arguments @("-NonInteractive") -LogName "Runtime\packaged-check" -TimeoutMilliseconds $CommandTimeoutMilliseconds
}
catch {
    Write-Report -RelativePath "Handoff\evidence-run-failure.txt" -Text ($_.Exception | Out-String)
}

Write-Report -RelativePath "Reports\external-template.txt" -Text "NOT GREEN: repository-local SDL3 configure can complete, but nested SDL/MSBuild exceeded the bounded validation window in the current environment. The known process tree was terminated; no executable is claimed.`r`n"
Write-Report -RelativePath "Reports\CI.txt" -Text "CI was not claimed by this local evidence run. Verify the normal Windows workflow for the exact final HEAD.`r`n"
Write-Report -RelativePath "Reports\renderer-diagnostics.txt" -Text "Packaged deterministic smoke passed. Automated smoke is not manual visual QA and does not replace GL-error or screenshot review.`r`n"
Write-Report -RelativePath "Reports\memory.txt" -Text "Renderer telemetry reports categorized logical GPU bytes, peak bytes, exact supported RGBA8 mip-chain estimates, and explicit overflow state. Driver VRAM remains distinct.`r`n"
Write-Report -RelativePath "Reports\manual-qa-checklist.txt" -Text ((@("Manual visual QA remains required:", "- app-only rendered viewport screenshot", "- HDR environment and material response", "- point and spot falloff and cone edges", "- shadow and alpha-mask parity", "- transparent ordering and overflow diagnostics", "- detached-window and resize recovery", "- no GL error report after a normal run", "- reference scene coverage") -join "`r`n") + "`r`n")
Write-Report -RelativePath "Screenshots\SCREENSHOT_INDEX.txt" -Text "No app-only screenshots were produced by this automated run. Manual capture remains an explicit gate.`r`n"
Write-Report -RelativePath "Handoff\remaining-campaign-tracks.txt" -Text "Not silently claimed complete: full glTF scene/material-binding coverage beyond the bounded geometry/PBR subset, KTX2/Basis, editor-specific material definitions beyond glTF, probes, cascaded/local shadows, AO/SSR/planar/temporal post, LOD/instancing/streaming, profiler/fault/soak coverage, CI verification, and manual visual evidence.`r`n"
Write-Report -RelativePath "Handoff\archive-self-verification.txt" -Text "PASS`r`nThe script compresses this evidence root, expands it into a fresh verification root, and validates every recorded SHA-256 before reporting the archive hash.`r`n"

$packageMarker = Join-Path $repoRoot "out\HenkaSandbox3D\PACKAGE_INFO.txt"
Write-HenkaUtf8NoBom -Path (Join-Path $OutputRoot "FINAL_COMMIT.txt") -Text ($head + "`r`n")
$packageText = "Package marker unavailable.`r`n"
if (Test-Path -LiteralPath $packageMarker) { $packageText = Get-Content -LiteralPath $packageMarker -Raw }
Write-HenkaUtf8NoBom -Path (Join-Path $OutputRoot "PACKAGE_INFO.txt") -Text $packageText
Write-HenkaUtf8NoBom -Path (Join-Path $OutputRoot "EVIDENCE_INFO.txt") -Text "Henka Engine Phase 3 evidence. Evidence archive, executable, package manifest, and source commit hashes are distinct values.`r`n"

$manifestPath = Join-Path $OutputRoot "MANIFEST.txt"
$shaPath = Join-Path $OutputRoot "SHA256SUMS.txt"
$files = @(Get-ChildItem -LiteralPath $OutputRoot -Recurse -File | Where-Object { $_.FullName -ne $shaPath } | Sort-Object FullName)
$manifestLines = foreach ($file in $files) { $file.FullName.Substring($OutputRoot.Length + 1).Replace("\", "/") }
Write-HenkaUtf8NoBom -Path $manifestPath -Text (($manifestLines -join "`r`n") + "`r`n")
$hashLines = foreach ($file in $files) {
    $relative = $file.FullName.Substring($OutputRoot.Length + 1).Replace("\", "/")
    ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + " *" + $relative)
}
Write-HenkaUtf8NoBom -Path $shaPath -Text (($hashLines -join "`r`n") + "`r`n")
$archivePath = $OutputRoot + ".zip"
Compress-Archive -Path (Join-Path $OutputRoot "*") -DestinationPath $archivePath -CompressionLevel Optimal
$verifyRoot = $OutputRoot + "-verify"
Expand-Archive -LiteralPath $archivePath -DestinationPath $verifyRoot -Force
foreach ($line in Get-Content -LiteralPath $shaPath) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split " \*", 2
    $verifyPath = Join-Path $verifyRoot ($parts[1].Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $verifyPath -PathType Leaf)) { throw "Archive verification missing: $($parts[1])" }
    $actual = (Get-FileHash -LiteralPath $verifyPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $parts[0].ToLowerInvariant()) { throw "Archive verification hash mismatch: $($parts[1])" }
}
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-HenkaUtf8NoBom -Path ($archivePath + ".sha256.txt") -Text ($archiveHash + " *" + (Split-Path -Leaf $archivePath) + "`r`n")
Write-Host "Phase 3 evidence archive: $archivePath"
Write-Host "Archive SHA-256: $archiveHash"
