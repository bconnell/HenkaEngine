Set-StrictMode -Version Latest

function Assert-HenkaPackageUserDataTreeHasNoReparsePoints {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $current = [System.IO.Path]::GetPathRoot($fullPath)
    $relative = $fullPath.Substring($current.Length)
    foreach ($part in @($relative -split "[\\/]" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
        $current = Join-Path $current $part
        $component = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($null -eq $component) { break }
        if (($component.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Package user-data path crosses a reparse point: $current"
        }
    }

    if (-not (Test-Path -LiteralPath $fullPath)) { return }
    $stack = New-Object 'System.Collections.Generic.Stack[System.IO.DirectoryInfo]'
    $root = Get-Item -LiteralPath $fullPath -Force
    if (($root.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Package user-data source contains a reparse point: $fullPath"
    }
    if ($root.PSIsContainer) { $stack.Push([System.IO.DirectoryInfo]$root) }
    while ($stack.Count -gt 0) {
        $directory = $stack.Pop()
        foreach ($item in $directory.EnumerateFileSystemInfos()) {
            $attributes = $item.Attributes
            if (($attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Package user-data source contains a reparse point: $($item.FullName)"
            }
            if (($attributes -band [System.IO.FileAttributes]::Directory) -ne 0) {
                $stack.Push([System.IO.DirectoryInfo]$item)
            }
        }
    }
}

function Copy-HenkaPackageUserDataForStaging {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [Parameter(Mandatory = $true)][string]$StagingRoot,
        [Parameter(Mandatory = $true)][string]$LegacyPackageUserPath,
        [switch]$ResetUserData,
        [switch]$AllowTestOwnedLegacyPath,
        [string]$TestOwnedPathJustification
    )

    $repository = [System.IO.Path]::GetFullPath($RepositoryRoot)
    $package = Resolve-HenkaLocalPath -RepositoryRoot $repository -Path $PackageRoot
    $staging = Resolve-HenkaLocalPath -RepositoryRoot $repository -Path $StagingRoot
    if (-not (Test-Path -LiteralPath $staging -PathType Container)) {
        throw "Package staging root must exist before user-data preparation: $staging"
    }

    $currentUser = Resolve-HenkaLocalPath -RepositoryRoot $repository -Path (Join-Path $package "user")
    $previousState = Resolve-HenkaLocalPath -RepositoryRoot $repository -Path (Join-Path $package ".henka-user-data-migration.json")
    $stagedUser = Join-Path $staging "user"
    $stagedState = Join-Path $staging ".henka-user-data-migration.json"
    $legacy = [System.IO.Path]::GetFullPath($LegacyPackageUserPath)
    $canonicalRepository = Get-HenkaCanonicalRepositoryRoot -RepositoryRoot $repository
    $expectedLegacy = [System.IO.Path]::GetFullPath((Join-Path $canonicalRepository "out\HenkaSandbox3D\user"))

    if (-not [string]::Equals($legacy, $expectedLegacy, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (-not $AllowTestOwnedLegacyPath -or [string]::IsNullOrWhiteSpace($TestOwnedPathJustification)) {
            throw "Legacy package user-data source is not the known old package location. Test-only alternate paths require an explicit justification."
        }
        $legacy = Resolve-HenkaLocalPath -RepositoryRoot $repository -Path $legacy
    }
    elseif ($AllowTestOwnedLegacyPath -and [string]::IsNullOrWhiteSpace($TestOwnedPathJustification)) {
        throw "Test-owned legacy source override requires a non-empty justification."
    }

    if (Test-Path -LiteralPath $stagedUser) {
        throw "Package staging already contains user data; refusing to merge or overwrite it: $stagedUser"
    }
    if (Test-Path -LiteralPath $stagedState) {
        throw "Package staging already contains migration state: $stagedState"
    }

    $action = "no-user-data"
    $source = $null
    if ($ResetUserData) {
        $action = "reset"
    }
    elseif (Test-Path -LiteralPath $currentUser -PathType Container) {
        $source = $currentUser
        $action = "preserved-package"
    }
    elseif (Test-Path -LiteralPath $previousState -PathType Leaf) {
        Assert-HenkaPackageUserDataTreeHasNoReparsePoints -Path $previousState
        try {
            $state = Get-Content -LiteralPath $previousState -Raw | ConvertFrom-Json -ErrorAction Stop
        }
        catch {
            throw "Existing package user-data migration state is invalid; refusing an implicit legacy import: $previousState"
        }
        if ($state.schema_version -ne 1 -or $state.action -notin @("legacy-copied", "preserved-package", "reset", "no-user-data", "migration-already-complete")) {
            throw "Existing package user-data migration state is unsupported; refusing an implicit legacy import: $previousState"
        }
        $action = "migration-already-complete"
    }
    elseif (Test-Path -LiteralPath $legacy -PathType Container) {
        $source = $legacy
        $action = "legacy-copied"
    }

    if ($null -ne $source) {
        Assert-HenkaPackageUserDataTreeHasNoReparsePoints -Path $source
        Copy-Item -LiteralPath $source -Destination $staging -Recurse -ErrorAction Stop | Out-Null
    }

    $stateDocument = [ordered]@{
        schema_version = 1
        action = $action
        recorded_utc = [DateTime]::UtcNow.ToString("o")
    }
    $json = ConvertTo-Json -InputObject $stateDocument -Depth 4
    [System.IO.File]::WriteAllText($stagedState, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    return $action
}
