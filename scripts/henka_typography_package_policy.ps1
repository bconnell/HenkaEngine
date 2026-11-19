Set-StrictMode -Version Latest

function Get-HenkaTypographyAssetSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        return [System.BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '')
    }
    finally {
        $stream.Dispose()
        $algorithm.Dispose()
    }
}

function Get-HenkaTypographyPackageAssetCatalog {
    return @(
        [pscustomobject]@{
            Name = 'AtkinsonHyperlegible-Regular.ttf'
            Sha256 = '7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669'
            Role = 'selected-font'
        },
        [pscustomobject]@{
            Name = 'Atkinson-OFL.txt'
            Sha256 = '64B9CAE8727CB41EA9E8843103E69647C82383F3A902E2BB39B2C5D92083B6E1'
            Role = 'selected-license'
        },
        [pscustomobject]@{
            Name = 'NotoSans-Regular.ttf'
            Sha256 = '478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823'
            Role = 'comparison-font'
        },
        [pscustomobject]@{
            Name = 'NotoSans-OFL.txt'
            Sha256 = 'E2E177A32561584D4FC13AAA3CD8E53758A12910F013FE9CA125419111722029'
            Role = 'comparison-license'
        }
    )
}

function Get-HenkaTypographySelectedFontMetadata {
    $catalog = @(Get-HenkaTypographyPackageAssetCatalog)
    $font = $catalog | Where-Object { $_.Role -eq 'selected-font' } | Select-Object -First 1
    $license = $catalog | Where-Object { $_.Role -eq 'selected-license' } | Select-Object -First 1
    if ($null -eq $font -or $null -eq $license) {
        throw 'The selected editor font package metadata is incomplete.'
    }
    return [pscustomobject]@{
        Name = 'Atkinson Hyperlegible Regular'
        RelativePath = "assets\ui\fonts\$($font.Name)"
        Sha256 = $font.Sha256
        LicenseRelativePath = "assets\ui\fonts\$($license.Name)"
        LicenseSha256 = $license.Sha256
    }
}

function Get-HenkaTypographyPackageAssetViolation {
    param(
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [switch]$IncludeComparisonFonts
    )

    $fontDirectory = Join-Path $PackageRoot 'assets\ui\fonts'
    if (-not (Test-Path -LiteralPath $fontDirectory -PathType Container)) {
        return 'The packaged UI font directory is missing.'
    }
    $fontDirectoryItem = Get-Item -LiteralPath $fontDirectory -Force
    if (($fontDirectoryItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        return 'The packaged UI font directory is a reparse point.'
    }

    $catalog = @(Get-HenkaTypographyPackageAssetCatalog)
    $expectedAssets = @($catalog | Where-Object { $_.Role -in @('selected-font', 'selected-license') })
    if ($IncludeComparisonFonts) {
        $expectedAssets += @($catalog | Where-Object { $_.Role -in @('comparison-font', 'comparison-license') })
    }

    foreach ($asset in $expectedAssets) {
        $path = Join-Path $fontDirectory $asset.Name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            return "Required typography asset is missing: $($asset.Name)."
        }
        $item = Get-Item -LiteralPath $path
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            return "Typography asset is a reparse point: $($asset.Name)."
        }
        $actualHash = (Get-HenkaTypographyAssetSha256 -Path $path).ToUpperInvariant()
        if ($actualHash -ne $asset.Sha256) {
            return "Typography asset hash differs from its pinned provenance: $($asset.Name)."
        }
    }

    $allowedFontNames = @($expectedAssets | Where-Object { $_.Name -match '\.(ttf|otf)$' } | ForEach-Object { $_.Name })
    foreach ($font in @(Get-ChildItem -LiteralPath $fontDirectory -File | Where-Object { $_.Extension -in @('.ttf', '.otf') })) {
        if (($font.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            return "Packaged font is a reparse point: $($font.Name)."
        }
        if ($allowedFontNames -notcontains $font.Name) {
            return "Unexpected font face is present in the package: $($font.Name)."
        }
    }

    if (-not $IncludeComparisonFonts) {
        foreach ($comparisonAsset in @('NotoSans-Regular.ttf', 'NotoSans-OFL.txt')) {
            if (Test-Path -LiteralPath (Join-Path $fontDirectory $comparisonAsset)) {
                return "The ordinary product package must not contain comparison asset $comparisonAsset."
            }
        }
    }

    return $null
}

function Assert-HenkaTypographyPackageAssetSet {
    param(
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [switch]$IncludeComparisonFonts
    )

    $violation = Get-HenkaTypographyPackageAssetViolation `
        -PackageRoot $PackageRoot `
        -IncludeComparisonFonts:$IncludeComparisonFonts
    if ($null -ne $violation) {
        throw "Packaged editor typography asset contract failed: $violation"
    }
}
