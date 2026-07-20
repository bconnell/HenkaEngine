Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$git = Get-HenkaGitPath
$selfPath = "scripts/check_repository_integrity.ps1"
$findings = New-Object System.Collections.Generic.List[string]

function Add-Finding {
    param([string]$Message)
    $findings.Add($Message)
}

function Convert-CodePoints {
    param([int[]]$Values)
    return -join @($Values | ForEach-Object { [char]$_ })
}

$trackedPaths = @(& $git -C $repoRoot ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to enumerate tracked repository files."
}
$trackedPaths = @($trackedPaths | ForEach-Object { ([string]$_).Trim().Replace("\", "/") })

$forbiddenRoots = @(
    "build/",
    "out/",
    ".vs/",
    "CMakeFiles/",
    "Testing/",
    "_deps/"
)
$forbiddenExactNames = @(
    ".env"
)
$forbiddenExtensions = @(
    ".exe", ".dll", ".lib", ".pdb", ".ilk",
    ".pfx", ".p12", ".pem", ".key", ".snk",
    ".suo", ".user", ".log", ".tmp", ".temp",
    ".cache", ".bak", ".orig"
)

foreach ($relativePath in $trackedPaths) {
    foreach ($root in $forbiddenRoots) {
        if ($relativePath.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-Finding "${relativePath}: generated or local-only path is tracked"
        }
    }

    $fileName = [System.IO.Path]::GetFileName($relativePath)
    $extension = [System.IO.Path]::GetExtension($relativePath).ToLowerInvariant()
    if ($fileName -in $forbiddenExactNames -or
        $fileName.StartsWith(".env.", [System.StringComparison]::OrdinalIgnoreCase)) {
        Add-Finding "${relativePath}: local environment file is tracked"
    }
    if ($extension -in $forbiddenExtensions) {
        Add-Finding "${relativePath}: private, generated, or binary extension is tracked"
    }
    if ($extension -eq ".obj" -and
        -not $relativePath.StartsWith("assets/models/", [System.StringComparison]::OrdinalIgnoreCase)) {
        Add-Finding "${relativePath}: object file is tracked outside the public model asset folder"
    }

    $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        Add-Finding "${relativePath}: tracked path is missing from the checkout"
        continue
    }

    if (-not $relativePath.StartsWith("third_party/", [System.StringComparison]::OrdinalIgnoreCase) -and
        (Get-Item -LiteralPath $absolutePath).Length -gt 10MB) {
        Add-Finding "${relativePath}: first-party file exceeds the 10 MiB repository limit"
    }
}

$textExtensions = @(
    ".c", ".h", ".cmake", ".md", ".txt", ".ps1", ".yml", ".yaml",
    ".json", ".vert", ".frag", ".gitignore", ".gitattributes",
    ".editorconfig", ".clang-format"
)

$privateKeyPattern = Convert-CodePoints @(45,45,45,45,45,66,69,71,73,78,32,40,82,83,65,124,69,67,124,79,80,69,78,83,83,72,124,80,82,73,86,65,84,69,41,63,32,80,82,73,86,65,84,69,32,75,69,89,45,45,45,45,45)
$classicTokenPrefix = Convert-CodePoints @(103,104,112,95)
$fineTokenPrefix = Convert-CodePoints @(103,105,116,104,117,98,95,112,97,116,95)
$cloudAccessPrefix = Convert-CodePoints @(65,75,73,65)
$secretAssignmentPattern = '(?i)\b(password|secret|api[_-]?key|access[_-]?token)\s*[:=]\s*["'']?[A-Za-z0-9+/=_-]{16,}'

foreach ($relativePath in $trackedPaths) {
    if ($relativePath.StartsWith("third_party/", [System.StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
    $fileName = [System.IO.Path]::GetFileName($relativePath)
    $extension = [System.IO.Path]::GetExtension($relativePath).ToLowerInvariant()
    $isText = $extension -in $textExtensions -or
        $fileName -in @("CMakeLists.txt", "LICENSE", ".gitignore", ".gitattributes", ".editorconfig", ".clang-format")
    if (-not $isText) {
        continue
    }

    $bytes = [System.IO.File]::ReadAllBytes($absolutePath)
    if ([Array]::IndexOf($bytes, [byte]0) -ge 0) {
        continue
    }

    $text = [System.IO.File]::ReadAllText($absolutePath)
    $lines = [System.IO.File]::ReadAllLines($absolutePath)
    for ($index = 0; $index -lt $lines.Length; ++$index) {
        if ($lines[$index] -match '^(<<<<<<<|=======|>>>>>>>)') {
            Add-Finding "${relativePath}:$($index + 1): merge-conflict marker"
        }
    }

    if ($text -match $privateKeyPattern) {
        Add-Finding "${relativePath}: private key material"
    }
    if ($text -match ([Regex]::Escape($classicTokenPrefix) + '[A-Za-z0-9]{36}')) {
        Add-Finding "${relativePath}: repository access token"
    }
    if ($text -match ([Regex]::Escape($fineTokenPrefix) + '[A-Za-z0-9_]{20,}')) {
        Add-Finding "${relativePath}: fine-grained repository access token"
    }
    if ($text -match ([Regex]::Escape($cloudAccessPrefix) + '[0-9A-Z]{16}')) {
        Add-Finding "${relativePath}: cloud access key"
    }
    if ($text -match $secretAssignmentPattern) {
        Add-Finding "${relativePath}: probable embedded credential assignment"
    }

    if ($extension -eq ".ps1") {
        $tokens = $null
        $parseErrors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            $absolutePath,
            [ref]$tokens,
            [ref]$parseErrors)
        foreach ($parseError in @($parseErrors)) {
            Add-Finding "${relativePath}:$($parseError.Extent.StartLineNumber): PowerShell parse error: $($parseError.Message)"
        }

        if ($relativePath -ne $selfPath) {
            $startProcessToken = Convert-CodePoints @(83,116,97,114,116,45,80,114,111,99,101,115,115)
            if ($text -match ('(?i)\b' + [Regex]::Escape($startProcessToken) + '\b')) {
                Add-Finding "${relativePath}: uses a process launcher that bypasses the shared native-process contract"
            }
            if ($text -match '(?im)^\s*return\s+if\s*\(') {
                Add-Finding "${relativePath}: uses return-if syntax that Windows PowerShell 5.1 executes incorrectly"
            }
        }
    }

    if ($extension -in @(".yml", ".yaml")) {
        foreach ($line in $lines) {
            if ($line -match '^\s*-\s*uses:\s*([^@\s]+)@([^\s#]+)') {
                $actionName = $Matches[1]
                $actionRef = $Matches[2]
                if (-not $actionName.StartsWith("./") -and $actionRef -notmatch '^[0-9a-f]{40}$') {
                    Add-Finding "${relativePath}: external action is not pinned to a full commit"
                }
            }
        }
    }
}

$rootCMake = [System.IO.File]::ReadAllText((Join-Path $repoRoot "CMakeLists.txt"))
if ($rootCMake -notmatch 'GIT_TAG\s+[0-9a-f]{40}') {
    Add-Finding "CMakeLists.txt: SDL dependency is not pinned to a full commit"
}

$gitignore = [System.IO.File]::ReadAllText((Join-Path $repoRoot ".gitignore"))
foreach ($requiredIgnore in @("*.pfx", "*.p12", "*.pem", "*.key", "*.snk")) {
    if (-not $gitignore.Contains($requiredIgnore)) {
        Add-Finding ".gitignore: missing private-key ignore pattern $requiredIgnore"
    }
}

if ($findings.Count -gt 0) {
    Write-Host "Repository integrity check failed:"
    $findings | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[pass] Repository integrity check passed."
