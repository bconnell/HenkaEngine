Set-StrictMode -Version Latest

function Get-HenkaTypographyProductSourceEquivalenceViolation {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$ChangedPaths)

    $validationScripts = @(
        'scripts/henka_ui_automation_helpers.ps1',
        'scripts/test_editor_typography_windows.ps1',
        'scripts/test_henka_ui_automation_helpers_windows.ps1',
        'scripts/henka_typography_capture_policy.ps1',
        'scripts/test_editor_typography_capture_policy_windows.ps1'
    )

    foreach ($pathValue in $ChangedPaths) {
        $path = ([string]$pathValue).Trim().Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($path) -or
            $path.StartsWith('/') -or
            $path -match '(^|/)\.\.?(/|$)') {
            return 'A changed source path is empty, rooted, or contains traversal segments.'
        }

        $validationOnly =
            $path.StartsWith('docs/design/', [System.StringComparison]::OrdinalIgnoreCase) -or
            $path -in @('docs/engine-completion-matrix.md', 'docs/ui.md') -or
            $path.StartsWith('tests/', [System.StringComparison]::OrdinalIgnoreCase) -or
            $validationScripts -contains $path
        if (-not $validationOnly) {
            return "Changed path is not approved as documentation, test, or typography-capture-only input: $path"
        }
    }

    return $null
}

function Get-HenkaTypographyProductSourceEquivalence {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$BuildCommitSha,
        [Parameter(Mandatory = $true)][string]$ValidationCommitSha
    )

    $repoPath = [System.IO.Path]::GetFullPath($RepositoryRoot)
    $git = Get-HenkaGitPath
    $diffOutput = @(& $git -C $repoPath diff --name-only --no-ext-diff --no-renames `
        $BuildCommitSha $ValidationCommitSha -- 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        $diagnostic = (($diffOutput | ForEach-Object { [string]$_ }) -join "`n")
        throw "Could not compare typography product source commits. Git exit=$exitCode; $diagnostic"
    }

    $changedPaths = @($diffOutput |
        ForEach-Object { [string]$_ } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $violation = Get-HenkaTypographyProductSourceEquivalenceViolation -ChangedPaths $changedPaths
    return [pscustomobject]@{
        policy_version = 'typography-product-source-equivalence-v1'
        build_commit_sha = $BuildCommitSha
        validation_commit_sha = $ValidationCommitSha
        equivalent = ($null -eq $violation)
        violation = $violation
        changed_paths = [string[]]$changedPaths
    }
}

function Get-HenkaTypographyCaptureScalePairViolation {
    param([Parameter(Mandatory = $true)][object[]]$Captures)

    if ($Captures.Count -eq 0) {
        return 'No typography captures were supplied.'
    }

    $groups = @{}
    foreach ($capture in $Captures) {
        if ($null -eq $capture -or
            $null -eq $capture.PSObject.Properties['font'] -or
            $null -eq $capture.PSObject.Properties['ui_scale_percent'] -or
            $null -eq $capture.PSObject.Properties['client_width'] -or
            $null -eq $capture.PSObject.Properties['client_height'] -or
            $null -eq $capture.PSObject.Properties['os_display_scale_numerator'] -or
            $null -eq $capture.PSObject.Properties['os_display_scale_denominator']) {
            return 'A typography capture is missing its product-reported OS display scale or comparison identity.'
        }

        try {
            $font = [string]$capture.font
            $scale = [int]$capture.ui_scale_percent
            $width = [int]$capture.client_width
            $height = [int]$capture.client_height
            $numerator = [uint32]$capture.os_display_scale_numerator
            $denominator = [uint32]$capture.os_display_scale_denominator
        }
        catch {
            return 'A typography capture contains a malformed scale or comparison identity.'
        }

        if ($font -notin @('atkinson', 'noto') -or
            $scale -notin @(100, 125, 150, 200) -or
            $width -le 0 -or $height -le 0 -or
            $numerator -eq 0 -or $denominator -eq 0) {
            return 'A typography capture contains an unsupported font, app scale, resolution, or OS display scale.'
        }

        $key = '{0}|{1}|{2}|{3}' -f $scale, $width, $height, $font
        if ($groups.ContainsKey($key)) {
            return "A duplicate typography capture exists for $key."
        }
        $groups[$key] = $capture
    }

    foreach ($scale in @(100, 125, 150, 200)) {
        $resolutions = @($Captures |
            Where-Object { $_.ui_scale_percent -eq $scale } |
            ForEach-Object { '{0}x{1}' -f $_.client_width, $_.client_height } |
            Sort-Object -Unique)
        foreach ($resolution in $resolutions) {
            $dimensions = $resolution -split 'x'
            $width = [int]$dimensions[0]
            $height = [int]$dimensions[1]
            $atkinsonKey = '{0}|{1}|{2}|atkinson' -f $scale, $width, $height
            $notoKey = '{0}|{1}|{2}|noto' -f $scale, $width, $height
            if (-not $groups.ContainsKey($atkinsonKey) -or -not $groups.ContainsKey($notoKey)) {
                return "Both fonts must have a matched capture at $scale percent, ${width}x${height}."
            }

            $atkinson = $groups[$atkinsonKey]
            $noto = $groups[$notoKey]
            if ([decimal]$atkinson.os_display_scale_numerator * [decimal]$noto.os_display_scale_denominator -ne
                [decimal]$noto.os_display_scale_numerator * [decimal]$atkinson.os_display_scale_denominator) {
                return "Atkinson and Noto captures used different product-reported OS display scales at $scale percent, ${width}x${height}."
            }
        }
    }

    return $null
}
