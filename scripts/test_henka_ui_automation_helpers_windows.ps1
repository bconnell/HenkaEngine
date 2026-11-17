param(
    [Parameter(Mandatory = $true)][string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'henka_script_common.ps1')
. (Join-Path $PSScriptRoot 'henka_ui_automation_helpers.ps1')

Add-Type -AssemblyName System.Drawing
$temporaryRoot = Get-HenkaTestTemporaryRoot -RepositoryRoot $RepositoryRoot
$testDirectory = Join-Path $temporaryRoot ('ui-bitmap-detach-' + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testDirectory) | Out-Null
$imagePath = Join-Path $testDirectory 'capture.bmp'
$telemetryPath = Join-Path $testDirectory 'authoring-rows.log'
$source = $null
$detached = $null
$exclusiveStream = $null
$liveLogWriter = $null

try {
    $source = New-Object System.Drawing.Bitmap -ArgumentList 3, 2
    $source.SetPixel(1, 1, [System.Drawing.Color]::FromArgb(255, 40, 80, 120))
    $source.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $source.Dispose()
    $source = $null

    $detached = New-HenkaDetachedBitmapFromFile -Path $imagePath -MaxDimension 16 -MaxPixels 256
    if ($detached.Width -ne 3 -or $detached.Height -ne 2) {
        throw 'The detached capture copy changed its source dimensions.'
    }
    $pixel = $detached.GetPixel(1, 1)
    if ($pixel.R -ne 40 -or $pixel.G -ne 80 -or $pixel.B -ne 120) {
        throw 'The detached capture copy changed a representative source pixel.'
    }

    try {
        $exclusiveStream = [System.IO.File]::Open(
            $imagePath,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None)
    }
    catch {
        throw 'The returned capture bitmap still holds the temporary source image file open.'
    }
    finally {
        if ($null -ne $exclusiveStream) {
            $exclusiveStream.Dispose()
            $exclusiveStream = $null
        }
    }

    [System.IO.File]::WriteAllLines($telemetryPath, @(
        'Native authoring source row: name=Showcase Giraffe Anatomical Giraffe Study Primitive x=12.0 y=20.0 width=180.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe Anatomical Giraffe Study Primitive x=14.0 y=24.0 width=184.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe [Study].Primitive x=30.5 y=40.5 width=90.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe malformed x=no y=1 width=10 height=28.0.'
    ))
    $exactRow = Get-HenkaNativeAuthoringSourceRowGeometry `
        -Path $telemetryPath `
        -Name 'Showcase Giraffe [Study].Primitive'
    if ($null -eq $exactRow -or $exactRow.X -ne 30.5 -or $exactRow.Y -ne 40.5 -or
        $exactRow.Name -ne 'Showcase Giraffe [Study].Primitive') {
        throw 'The exact native authoring row parser did not return escaped-name geometry.'
    }
    $liveLogWriter = [System.IO.File]::Open(
        $telemetryPath,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::Read)
    $liveLogWriter.Position = $liveLogWriter.Length
    $prefixRow = Get-HenkaNativeAuthoringSourceRowGeometry `
        -Path $telemetryPath `
        -NamePrefix 'Showcase Giraffe Anatomical' `
        -Latest
    $liveLogWriter.Dispose()
    $liveLogWriter = $null
    if ($null -eq $prefixRow -or $prefixRow.X -ne 14.0 -or $prefixRow.Y -ne 24.0 -or
        $prefixRow.Width -ne 184.0) {
        throw 'The prefix native authoring row parser did not return the latest complete product geometry.'
    }
    $missingRow = Get-HenkaNativeAuthoringSourceRowGeometry -Path $telemetryPath -Name 'No matching row'
    if ($null -ne $missingRow) {
        throw 'The native authoring row parser fabricated geometry for a missing row.'
    }

    Write-Output 'Henka UI automation helper tests passed.'
}
finally {
    if ($null -ne $exclusiveStream) { $exclusiveStream.Dispose() }
    if ($null -ne $liveLogWriter) { $liveLogWriter.Dispose() }
    if ($null -ne $detached) { $detached.Dispose() }
    if ($null -ne $source) { $source.Dispose() }
    if (Test-Path -LiteralPath $testDirectory -PathType Container) {
        $rootItem = Get-Item -LiteralPath $testDirectory -Force
        if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to remove a reparse-point test directory: $testDirectory"
        }
        Remove-Item -LiteralPath $testDirectory -Recurse -Force -ErrorAction Stop
        if (Test-Path -LiteralPath $testDirectory) {
            throw "The exact UI capture test directory remains after cleanup: $testDirectory"
        }
    }
}
