param(
    [Parameter(Mandatory = $true)]
    [string]$CandidateCommitSubject,

    [Parameter(Mandatory = $true)]
    [string]$SliceName,

    [string[]]$SourceAnchor = @(),

    [string[]]$ExpectedChangedPath = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion.Major -ne 5) {
    throw "This workflow requires Windows PowerShell 5.1."
}

$repoRoot = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
$orchestratorPath = Join-Path $PSScriptRoot "validated_windows_slice.ps1"
$runnerPath = $MyInvocation.MyCommand.Path
$manifestPath = Join-Path $PSScriptRoot "validated_windows_slice.sha256"

function Assert-PowerShellFileParses {
    param([Parameter(Mandatory = $true)][string]$Path)

    $tokens = $null
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $Path,
        [ref]$tokens,
        [ref]$errors)
    if (@($errors).Count -ne 0) {
        $messages = @($errors | ForEach-Object { $_.Message }) -join "; "
        throw "PowerShell parsing failed for ${Path}: $messages"
    }
}

function Assert-WorkflowManifest {
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "The validation workflow manifest is missing."
    }

    $expectedPaths = @(
        "scripts/invoke_validated_windows_slice.ps1",
        "scripts/validated_windows_slice.ps1"
    )
    $seen = @{}
    foreach ($line in @([System.IO.File]::ReadAllLines($manifestPath))) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line -notmatch '^([0-9a-fA-F]{64})  ([^\r\n]+)$') {
            throw "The validation workflow manifest contains a malformed entry."
        }
        $relativePath = $Matches[2].Replace("\", "/")
        if ($relativePath -notin $expectedPaths -or $seen.ContainsKey($relativePath)) {
            throw "The validation workflow manifest contains an unexpected or duplicate path: $relativePath"
        }
        $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
        if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
            throw "A validation workflow file is missing: $relativePath"
        }
        $actualHash = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $Matches[1].ToLowerInvariant()) {
            throw "Validation workflow hash mismatch: $relativePath"
        }
        $seen[$relativePath] = $true
    }
    foreach ($expectedPath in $expectedPaths) {
        if (-not $seen.ContainsKey($expectedPath)) {
            throw "The validation workflow manifest omits: $expectedPath"
        }
    }
}

Assert-PowerShellFileParses -Path $runnerPath
Assert-PowerShellFileParses -Path $orchestratorPath
Assert-WorkflowManifest

try {
    & $orchestratorPath `
        -CandidateCommitSubject $CandidateCommitSubject `
        -SliceName $SliceName `
        -SourceAnchor $SourceAnchor `
        -ExpectedChangedPath $ExpectedChangedPath
    if ($LASTEXITCODE -ne 0) {
        throw "Validated Windows slice returned exit code $LASTEXITCODE."
    }
    Write-Host "[pass] Validated Windows slice entrypoint completed."
    exit 0
}
catch {
    Write-Host "[fail] Validated Windows slice entrypoint failed: $($_.Exception.Message)"
    exit 1
}
