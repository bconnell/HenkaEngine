Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
if ($null -eq $gitCommand) {
    $gitCommand = Get-Command git -ErrorAction SilentlyContinue
}
if ($null -eq $gitCommand) {
    throw "Git was not found."
}

function Convert-CodePoints {
    param([int[]]$Values)

    return -join @($Values | ForEach-Object { [char]$_ })
}

function New-WholeWordPattern {
    param(
        [string]$Token,
        [switch]$CaseSensitive
    )

    $escaped = [System.Text.RegularExpressions.Regex]::Escape($Token)
    $pattern = "(?<![A-Za-z0-9])$escaped(?![A-Za-z0-9])"
    $options = [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    if (-not $CaseSensitive) {
        $options = $options -bor [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    }

    return [pscustomobject]@{ Pattern = $pattern; Options = $options }
}

$ruleSpecs = @(
    @{ Name = "unfinished public wording"; Token = Convert-CodePoints @(112, 108, 97, 99, 101, 104, 111, 108, 100, 101, 114); CaseSensitive = $false },
    @{ Name = "internal generation reference 1"; Token = Convert-CodePoints @(65, 73); CaseSensitive = $false },
    @{ Name = "internal generation reference 2"; Token = Convert-CodePoints @(76, 76, 77); CaseSensitive = $false },
    @{ Name = "internal generation reference 3"; Token = Convert-CodePoints @(67, 104, 97, 116, 71, 80, 84); CaseSensitive = $false },
    @{ Name = "internal generation reference 4"; Token = Convert-CodePoints @(67, 111, 100, 101, 120); CaseSensitive = $false },
    @{ Name = "internal generation reference 5"; Token = Convert-CodePoints @(79, 112, 101, 110, 65, 73); CaseSensitive = $false },
    @{ Name = "internal generation reference 6"; Token = Convert-CodePoints @(97, 114, 116, 105, 102, 105, 99, 105, 97, 108, 32, 105, 110, 116, 101, 108, 108, 105, 103, 101, 110, 99, 101); CaseSensitive = $false },
    @{ Name = "internal generation reference 7"; Token = Convert-CodePoints @(108, 97, 110, 103, 117, 97, 103, 101, 32, 109, 111, 100, 101, 108); CaseSensitive = $false },
    @{ Name = "internal generation reference 8"; Token = Convert-CodePoints @(99, 111, 100, 105, 110, 103, 32, 97, 103, 101, 110, 116); CaseSensitive = $false },
    @{ Name = "internal generation reference 9"; Token = Convert-CodePoints @(103, 101, 110, 101, 114, 97, 116, 101, 100, 32, 98, 121); CaseSensitive = $false }
)

$rules = @()
foreach ($spec in $ruleSpecs) {
    $compiled = New-WholeWordPattern -Token $spec.Token -CaseSensitive:$spec.CaseSensitive
    $rules += [pscustomobject]@{
        Name = $spec.Name
        Pattern = $compiled.Pattern
        Options = $compiled.Options
    }
}

$textExtensions = @(
    ".c", ".h", ".cmake", ".md", ".txt", ".ps1", ".yml", ".yaml",
    ".json", ".obj", ".vert", ".frag", ".gitignore", ".gitattributes",
    ".editorconfig", ".clang-format"
)

$gitArguments = @("-C", $repoRoot, "ls-files", "--cached", "--others", "--exclude-standard")
$paths = @(& $gitCommand.Source @gitArguments)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to enumerate repository files."
}

$findings = New-Object System.Collections.Generic.List[string]
foreach ($rawPath in $paths) {
    $relativePath = ([string]$rawPath).Trim().Replace("\", "/")
    if ([string]::IsNullOrWhiteSpace($relativePath) -or
        $relativePath.StartsWith("third_party/", [System.StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    $fileName = [System.IO.Path]::GetFileName($relativePath)
    $extension = [System.IO.Path]::GetExtension($relativePath).ToLowerInvariant()
    $isKnownText = $extension -in $textExtensions -or
        $fileName -in @("CMakeLists.txt", "LICENSE", ".gitignore", ".gitattributes", ".editorconfig", ".clang-format")
    if (-not $isKnownText) {
        continue
    }

    $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        continue
    }

    $bytes = [System.IO.File]::ReadAllBytes($absolutePath)
    if ([Array]::IndexOf($bytes, [byte]0) -ge 0) {
        continue
    }

    $lines = [System.IO.File]::ReadAllLines($absolutePath)
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; ++$lineIndex) {
        foreach ($rule in $rules) {
            if ([System.Text.RegularExpressions.Regex]::IsMatch(
                    $lines[$lineIndex],
                    $rule.Pattern,
                    $rule.Options)) {
                $findings.Add("${relativePath}:$($lineIndex + 1): $($rule.Name)")
            }
        }
    }
}

if ($findings.Count -gt 0) {
    Write-Host "Repository hygiene check failed:"
    $findings | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[pass] Public repository hygiene check passed."
