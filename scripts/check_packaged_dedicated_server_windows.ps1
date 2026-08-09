param(
    [int]$Port = 7813
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$packageRoot = Join-Path $repoRoot "out\HenkaDedicatedServer"
$packageExe = Join-Path $packageRoot "henka_dedicated_server.exe"
$packageInfo = Join-Path $packageRoot "PACKAGE_INFO.txt"
$saveRoot = Join-Path $packageRoot "save"

function Get-PackageValue {
    param([string]$Name)
    $match = Select-String -LiteralPath $packageInfo -Pattern ("^" + [Regex]::Escape($Name) + ":\s*(.+)$") | Select-Object -First 1
    if ($null -eq $match) { throw "Package field was not found: $Name" }
    return $match.Matches[0].Groups[1].Value.Trim()
}

if ($Port -lt 1 -or $Port -gt 65535) { throw "Port must be in the range 1-65535." }
foreach ($relative in @("henka_dedicated_server.exe", "server.conf.example", "README.txt", "docs\dedicated-server.md", "PACKAGE_INFO.txt", "save")) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $relative))) { throw "Required package path is missing: $relative" }
}
$forbidden = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object { $_.Name -match "HenkaSandbox3D|sandbox3d|SDL|ktx|KTX|opengl|OpenGL|renderer|\.pdb$" })
if ($forbidden.Count -gt 0) { throw "The headless package contains a graphical or debug artifact: $($forbidden[0].FullName)" }
$allowedRootNames = @("henka_dedicated_server.exe", "server.conf.example", "README.txt", "PACKAGE_INFO.txt", "docs", "save")
foreach ($item in @(Get-ChildItem -LiteralPath $packageRoot -Force)) {
    if ($allowedRootNames -notcontains $item.Name) { throw "Unexpected root package item: $($item.Name)" }
}
foreach ($textPath in @((Join-Path $packageRoot "server.conf.example"), (Join-Path $packageRoot "README.txt"), (Join-Path $packageRoot "PACKAGE_INFO.txt"), (Join-Path $packageRoot "docs\dedicated-server.md"))) {
    $text = Get-Content -LiteralPath $textPath -Raw
    $userPathPattern = [Regex]::Escape(("C:" + [char]92 + "Users" + [char]92))
    $tempPathPattern = [Regex]::Escape(([char]92 + "Temp" + [char]92))
    $sourcePathPattern = [Regex]::Escape(("source" + [char]92 + "repos"))
    if ($text -match (("(?i)" + $userPathPattern) + "|AppData|" + $tempPathPattern + "|" + $sourcePathPattern)) {
        throw "Package text contains a machine-specific or source-tree path: $textPath"
    }
}
$git = Get-HenkaGitPath
$currentCommit = ([string](& $git -C $repoRoot rev-parse HEAD)).Trim()
if ($LASTEXITCODE -ne 0 -or $currentCommit -ne (Get-PackageValue "Source commit")) { throw "Package source commit does not match the current checkout." }
$sourceState = if (@(& $git -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null).Count -eq 0) { "clean" } else { "working-tree" }
if ($LASTEXITCODE -ne 0 -or $sourceState -ne (Get-PackageValue "Source state")) { throw "Package source state does not match the current checkout." }
$packagedHash = (Get-FileHash -LiteralPath $packageExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($packagedHash -ne (Get-PackageValue "Packaged executable SHA-256") -or $packagedHash -ne (Get-PackageValue "Source executable SHA-256")) { throw "Package executable provenance hash failed." }

function Invoke-ServerSmoke {
    param([string]$Label)
    $result = Invoke-HenkaNativeCapture `
        -FilePath $packageExe `
        -Arguments @("--smoke", "--bind", "127.0.0.1", "--port", [string]$Port, "--save-root", "save", "--config", "server.conf.example") `
        -WorkingDirectory $packageRoot `
        -Label $Label `
        -TimeoutMilliseconds 30000
    if ($result.Stdout -notmatch "Henka dedicated server running: 127\.0\.0\.1:$Port") { throw "$Label did not prove the requested bind." }
    if ($result.Stdout -notmatch "loopback client connected") { throw "$Label did not prove loopback client connectivity." }
    if ($result.Stdout -notmatch "terrain revision [1-9][0-9]*") { throw "$Label did not report a recovered or committed Terrain revision." }
    if ($result.Stdout -notmatch "Dedicated server smoke initialized") { throw "$Label did not reach clean smoke completion." }
    return $result.Stdout
}

Write-Host "[check] Running packaged dedicated-server startup, bind, loopback, and persistence smoke"
$first = Invoke-ServerSmoke -Label "First packaged dedicated-server smoke"
$regionPath = Join-Path $saveRoot "region_0_0.htr"
if (-not (Test-Path -LiteralPath $regionPath -PathType Leaf)) { throw "The smoke edit did not leave a committed region snapshot." }
$second = Invoke-ServerSmoke -Label "Restarted packaged dedicated-server smoke"
$firstMatch = [Regex]::Match($first, "terrain revision ([1-9][0-9]*)")
$secondMatch = [Regex]::Match($second, "terrain revision ([1-9][0-9]*)")
if (-not $firstMatch.Success -or -not $secondMatch.Success) { throw "Terrain revision output was malformed." }
$firstRevision = [uint64]$firstMatch.Groups[1].Value
$secondRevision = [uint64]$secondMatch.Groups[1].Value
if ($firstRevision -ne $secondRevision) { throw "Restart did not restore the same committed Terrain revision." }
Write-Host "[pass] Dedicated-server package contains only headless inputs and passed startup, bind, loopback, clean shutdown, and restart persistence checks at revision $secondRevision."
