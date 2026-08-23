Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$findings = New-Object System.Collections.Generic.List[string]

function Add-Finding {
    param([string]$Message)
    $findings.Add($Message)
}

function Read-RepositoryText {
    param([string]$RelativePath)

    $absolutePath = Join-Path $repoRoot ($RelativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        Add-Finding "${RelativePath}: required authority file is missing"
        return ""
    }
    return [System.IO.File]::ReadAllText($absolutePath)
}

$compilerHeader = Read-RepositoryText "engine/include/henka/henkascript.h"
$compilerSource = Read-RepositoryText "engine/src/scripting/henkascript.c"
$assetSource = Read-RepositoryText "engine/src/scripting/script_asset.c"
$documentation = Read-RepositoryText "docs/scripting-foundation.md"
$backendPaths = @(
    "engine/src/scripting/henkascript_backend.c",
    "engine/src/scripting/lua_backend.c"
)

if ($compilerHeader -notmatch 'henka_hks_token_kind_get_class') {
    Add-Finding "engine/include/henka/henkascript.h: compiler token presentation API is missing"
}
if ($compilerHeader -notmatch 'henka_hks_get_default_behavior_source') {
    Add-Finding "engine/include/henka/henkascript.h: compiler-owned behavior template API is missing"
}
if ($compilerHeader -notmatch 'henka_hks_token_stream_get_indent_level') {
    Add-Finding "engine/include/henka/henkascript.h: compiler-owned indentation API is missing"
}
if ($compilerSource -notmatch 'static const struct keyword keywords\[\]') {
    Add-Finding "engine/src/scripting/henkascript.c: compiler keyword table is missing"
}
if ($assetSource -notmatch 'henka_hks_get_default_behavior_source') {
    Add-Finding "engine/src/scripting/script_asset.c: HKS template does not call the compiler-owned source API"
}
if ($assetSource -match '(?m)fn\s+On[A-Za-z0-9_]*\s*\(') {
    Add-Finding "engine/src/scripting/script_asset.c: contains a copied HenkaScript lifecycle template"
}

$lifecycleNamePattern = '"On(?:Create|Start|Update|Stop|Event|FixedUpdate|Interact|CollisionEnter|CollisionStay|CollisionExit|TriggerEnter|TriggerStay|TriggerExit|Destroy)"'
foreach ($relativePath in $backendPaths) {
    $backendSource = Read-RepositoryText $relativePath
    if ($backendSource -notmatch 'henka_script_lifecycle_schema_(get|find)') {
        Add-Finding "${relativePath}: backend is not visibly connected to the compiler/runtime lifecycle registry"
    }
    if ($backendSource -match $lifecycleNamePattern) {
        Add-Finding "${relativePath}: contains a copied lifecycle-name table; use the shared lifecycle registry"
    }
}

$editorPaths = @(
    "examples/sandbox3d/script_editor.c"
)

foreach ($relativePath in $editorPaths) {
    $text = Read-RepositoryText $relativePath
    if ($text -match 'HENKA_HKS_TOKEN_KW_') {
        Add-Finding "${relativePath}: editor references compiler keyword token constants directly"
    }
    if ($text -match '(?m)henka_hks_keyword_kind\s*\(') {
        Add-Finding "${relativePath}: editor contains a copied keyword classifier"
    }
    if ($text -match '(?m)\{\s*"(?:bool|i32|u32|f32|vec3|entity|fn|behavior|return|emit|if|else|while|for|break|continue|let|var)"\s*,') {
        Add-Finding "${relativePath}: editor contains a copied HenkaScript keyword table"
    }
    if ($text -notmatch 'henka_hks_(lex|token_kind_get_class|token_stream_get_indent_level)') {
        Add-Finding "${relativePath}: editor source is not visibly connected to the compiler-owned syntax seam"
    }
}

foreach ($requiredPhrase in @(
    'compiler remains the authority for HenkaScript syntax',
    'compiler''s keyword table or grammar',
    'not a second grammar or template',
    'henka_hks_get_default_behavior_source'
)) {
    if ($documentation -notmatch [Regex]::Escape($requiredPhrase)) {
        Add-Finding "docs/scripting-foundation.md: missing compiler-authority contract '$requiredPhrase'"
    }
}

if ($findings.Count -gt 0) {
    Write-Host "Scripting authority check failed:"
    $findings | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[pass] Scripting compiler-authority check passed."
