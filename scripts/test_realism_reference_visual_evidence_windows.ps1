$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$checker = Join-Path $PSScriptRoot 'check_realism_reference_visual_evidence_windows.ps1'
$tempRoot = Join-Path $repoRoot ('build\test_tmp\realism-reference-visual-validator-' + [Guid]::NewGuid().ToString('N'))

function New-ReferenceBitmap {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$Rendered
    )

    $bitmap = [System.Drawing.Bitmap]::new(1280, 720)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(236, 226, 216))
        $graphics.FillRectangle(
            [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(112, 118, 132)),
            0, 500, 1280, 220)

        $centers = @(
            @(408, 169), @(648, 169), @(887, 169),
            @(408, 391), @(648, 391), @(887, 391),
            @(408, 613), @(648, 613), @(887, 613)
        )
        $colors = @(
            @(242, 242, 242), @(18, 28, 48), @(210, 42, 42),
            @(22, 177, 215), @(164, 78, 42), @(210, 112, 174),
            @(164, 58, 34), @(126, 76, 42), @(244, 112, 22)
        )
        for ($index = 0; $index -lt $centers.Count; ++$index) {
            $rgb = $colors[$index]
            if (-not $Rendered) {
                $rgb = @(
                    [Math]::Min(255, $rgb[0] + 8),
                    [Math]::Min(255, $rgb[1] + 8),
                    [Math]::Min(255, $rgb[2] + 8)
                )
            }
            $brush = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb($rgb[0], $rgb[1], $rgb[2]))
            try {
                $graphics.FillEllipse(
                    $brush,
                    $centers[$index][0] - 76,
                    $centers[$index][1] - 76,
                    152,
                    152)
            }
            finally {
                $brush.Dispose()
            }
        }

        if ($Rendered) {
            $shadowBrush = [System.Drawing.SolidBrush]::new(
                [System.Drawing.Color]::FromArgb(150, 18, 22, 32))
            try {
                $graphics.FillEllipse($shadowBrush, 505, 650, 286, 52)
            }
            finally {
                $shadowBrush.Dispose()
            }
        }
    }
    finally {
        $graphics.Dispose()
    }
    try {
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function Write-ReferenceIndex {
    param([Parameter(Mandatory = $true)][string]$Directory)

    $metadata = 'reference_layout=close_grid reference_texture_edge=128 reference_exposure_stops=0.0000 viewport=0,0,1280,720 aspect=1.777778 camera_position=0.0000,1.9000,0.7872 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-2.8000,1.8500,1.7500,0.5000 reference_midpoint=640.00,360.00 reference_count=9 settled_frames=3 draw_expected=1'
    @(
        'Evidence profile: REALISM_REFERENCE',
        "CAPTURE_READY_REFERENCE mode=solid view=close $metadata",
        "CAPTURE_READY_REFERENCE mode=material_preview view=close $metadata",
        "CAPTURE_READY_REFERENCE mode=rendered view=close $metadata"
    ) | Set-Content -LiteralPath (Join-Path $Directory 'INDEX.txt')
}

function Invoke-ReferenceChecker {
    param([Parameter(Mandatory = $true)][string]$Directory)

    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $checker -InputDirectory $Directory 2>&1
        )
    }
    finally {
        $ErrorActionPreference = $previousErrorAction
    }
    [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = ($output -join [Environment]::NewLine)
    }
}

Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
try {
    New-ReferenceBitmap (Join-Path $tempRoot 'realism-reference-close-solid.png') $false
    New-ReferenceBitmap (Join-Path $tempRoot 'realism-reference-close-material-preview.png') $false
    New-ReferenceBitmap (Join-Path $tempRoot 'realism-reference-close-rendered.png') $true
    Write-ReferenceIndex $tempRoot

    $baseline = Invoke-ReferenceChecker $tempRoot
    if ($baseline.ExitCode -ne 0) {
        throw "Realism visual validator baseline unexpectedly failed: $($baseline.Output)"
    }

    $renderedPath = Join-Path $tempRoot 'realism-reference-close-rendered.png'
    $negativePath = Join-Path $tempRoot 'realism-reference-close-rendered-negative.png'
    $bitmap = [System.Drawing.Bitmap]::new($renderedPath)
    try {
        $centerX = [int][Math]::Round(1280 * 0.506)
        $centerY = [int][Math]::Round(720 * 0.543)
        for ($y = $centerY - 22; $y -lt $centerY + 22; $y += 2) {
            for ($x = $centerX - 22; $x -lt $centerX + 22; $x += 2) {
                $value = if ((($x - $centerX) / 2 + ($y - $centerY) / 2) % 2 -eq 0) { 245 } else { 8 }
                $color = [System.Drawing.Color]::FromArgb($value, $value, $value)
                $bitmap.SetPixel($x, $y, $color)
                $bitmap.SetPixel($x + 1, $y, $color)
                $bitmap.SetPixel($x, $y + 1, $color)
                $bitmap.SetPixel($x + 1, $y + 1, $color)
            }
        }
        $bitmap.Save($negativePath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
    Move-Item -LiteralPath $negativePath -Destination $renderedPath -Force

    $negative = Invoke-ReferenceChecker $tempRoot
    if ($negative.ExitCode -eq 0) {
        throw 'Realism visual validator negative control unexpectedly passed.'
    }
    if ($negative.Output -notmatch 'excessive detail structure') {
        throw "Realism visual validator negative control failed for the wrong reason: $($negative.Output)"
    }

    Write-Output 'Realism visual validator baseline and detail-structure negative control passed.'
}
finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
