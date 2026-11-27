param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$probeRoot = New-HenkaTemporaryDirectory -RepositoryRoot $RepositoryRoot -Purpose "documentation-truth"
$copyTargets = @(
    "docs",
    "scripts\henka_script_common.ps1",
    "scripts\check_documentation_truth.ps1",
    "scripts\package_dedicated_server_windows.ps1",
    "scripts\capture_visual_evidence_windows.ps1",
    "README.md",
    "SECURITY.md",
    "SUPPORT.md",
    "CONTRIBUTING.md",
    "LICENSE",
    "engine\include\henka\scene_document.h",
    "engine\src\core\scene_document.c",
    "examples\sandbox3d\main.c",
    "examples\sandbox3d\project_scene_catalog.c",
    "templates\external_game_minimal\src\main.c",
    "engine\src\mesh\authoring_mesh.c"
)

function Invoke-TruthCheck {
    $checker = Join-Path $probeRoot "scripts\check_documentation_truth.ps1"
    return Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-File", $checker) `
        -WorkingDirectory $probeRoot `
        -Label "Run documentation truth check" `
        -TimeoutMilliseconds 120000
}

try {
    foreach ($relativePath in $copyTargets) {
        $source = Join-Path $RepositoryRoot $relativePath
        $destination = Join-Path $probeRoot $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
    }

    $git = (Get-Command git.exe -ErrorAction Stop).Source
    & $git -C $probeRoot init --quiet
    & $git -C $probeRoot config user.email "truth-test@example.invalid"
    & $git -C $probeRoot config user.name "Documentation Truth Test"
    & $git -c core.autocrlf=false -C $probeRoot add --all

    $authoringMeshPath = Join-Path $probeRoot "docs\authoring-mesh.md"
    $readmePath = Join-Path $probeRoot "README.md"
    $readmeSource = [System.IO.File]::ReadAllText($readmePath)

    if ((Invoke-TruthCheck) -ne 0) {
        throw "Documentation truth baseline unexpectedly failed."
    }

    $audioPath = Join-Path $probeRoot "docs\audio.md"
    $audioSource = [System.IO.File]::ReadAllText($audioPath)
    [System.IO.File]::AppendAllText(
        $audioPath,
        [Environment]::NewLine + "Current Audio writer format: HSCN v8." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale Audio writer-format claim."
    }
    Write-Host "[pass] Documentation truth rejected a stale Audio writer-format claim."
    [System.IO.File]::WriteAllText($audioPath, $audioSource)

    $matrixPath = Join-Path $probeRoot "docs\engine-completion-matrix.md"
    $matrixSource = [System.IO.File]::ReadAllText($matrixPath)
    $matrixAudioProbe = [regex]::Replace(
        $matrixSource,
        '(?i)Current HSCN v\d+ writer; Audio emitter fields introduced in v\d+ and listener in v\d+(?:[;|.])',
        'Scene Document v6',
        1)
    if ($matrixAudioProbe -eq $matrixSource) {
        throw "Documentation truth regression could not locate the Audio matrix format claim."
    }
    [System.IO.File]::WriteAllText($matrixPath, $matrixAudioProbe)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale Audio matrix version claim."
    }
    Write-Host "[pass] Documentation truth rejected a stale Audio matrix version claim."
    [System.IO.File]::WriteAllText($matrixPath, $matrixSource)

    $runtimeFoundationsPath = Join-Path $probeRoot "docs\runtime-foundations.md"
    $runtimeFoundationsSource = [System.IO.File]::ReadAllText($runtimeFoundationsPath)
    [System.IO.File]::AppendAllText(
        $runtimeFoundationsPath,
        [Environment]::NewLine + "Current render-resource writer format: HSCN v12." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale render-resource writer-format claim."
    }
    Write-Host "[pass] Documentation truth rejected a stale render-resource writer-format claim."
    [System.IO.File]::WriteAllText($runtimeFoundationsPath, $runtimeFoundationsSource)

    $sceneArchitecturePath = Join-Path $probeRoot "docs\scene-authoring-architecture.md"
    $sceneArchitectureSource = [System.IO.File]::ReadAllText($sceneArchitecturePath)
    [System.IO.File]::AppendAllText(
        $sceneArchitecturePath,
        [Environment]::NewLine + "Future loaders may migrate older HSCN formats." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted obsolete future-tense HSCN migration guidance."
    }
    Write-Host "[pass] Documentation truth rejected obsolete future-tense HSCN migration guidance."
    [System.IO.File]::WriteAllText($sceneArchitecturePath, $sceneArchitectureSource)

    $editorControlsPath = Join-Path $probeRoot "docs\editor-controls.md"
    $editorControlsSource = [System.IO.File]::ReadAllText($editorControlsPath)
    $withoutInspectorGuidance = [regex]::Replace(
        $editorControlsSource,
        '(?ms)^## Numeric World Transform Inspector\r?\n.*?(?=^## |\z)',
        "")
    [System.IO.File]::WriteAllText($editorControlsPath, $withoutInspectorGuidance)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted missing numeric World Transform Inspector guidance."
    }
    Write-Host "[pass] Documentation truth rejected missing numeric World Transform Inspector guidance."
    [System.IO.File]::WriteAllText($editorControlsPath, $editorControlsSource)

    $mcpHarnessPath = Join-Path $probeRoot "docs\mcp-qa-harness.md"
    $mcpHarnessSource = [System.IO.File]::ReadAllText($mcpHarnessPath)
    $withoutVisualEvidenceBoundary = [regex]::Replace(
        $mcpHarnessSource,
        '(?ms)^## Visual evidence boundary\r?\n.*?(?=^## |\z)',
        "")
    [System.IO.File]::WriteAllText($mcpHarnessPath, $withoutVisualEvidenceBoundary)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted an omitted application-owned framebuffer evidence boundary."
    }
    Write-Host "[pass] Documentation truth rejected the missing application-owned framebuffer evidence boundary."
    [System.IO.File]::WriteAllText($mcpHarnessPath, $mcpHarnessSource)

    $withoutMcpScopeClarification = [regex]::Replace(
        $mcpHarnessSource,
        '(?s)This MCP validation slice does not add framebuffer-capture, save, or reload\s+operations to the adapter\. The packaged capture workflow above is separate\s+from MCP; scene save and reload are exercised through their own product paths\.',
        "The packaged capture workflow above is separate from MCP.")
    if ($withoutMcpScopeClarification -eq $mcpHarnessSource) {
        throw "Documentation regression could not remove the MCP scope clarification."
    }
    [System.IO.File]::WriteAllText($mcpHarnessPath, $withoutMcpScopeClarification)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted ambiguous MCP versus packaged capture scope."
    }
    Write-Host "[pass] Documentation truth rejected ambiguous MCP versus packaged capture scope."
    [System.IO.File]::WriteAllText($mcpHarnessPath, $mcpHarnessSource)

    $staleGameAuthoringRow = [regex]::Replace(
        $readmeSource,
        '(?m)^\|\s*Game authoring\s*\|[^\r\n]*$',
        '| Game authoring | Foundation | Full Game/Play authoring is not yet available |',
        1)
    if ($staleGameAuthoringRow -eq $readmeSource) {
        throw "Documentation truth regression could not locate the Game Authoring summary row."
    }
    [System.IO.File]::WriteAllText($readmePath, $staleGameAuthoringRow)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale Game Authoring claim that obscures implemented workflows."
    }
    Write-Host "[pass] Documentation truth regression rejected a stale Game Authoring capability claim."
    [System.IO.File]::WriteAllText($readmePath, $readmeSource)

    $roadmapPath = Join-Path $probeRoot "docs\roadmap.md"
    $roadmapSource = [System.IO.File]::ReadAllText($roadmapPath)
    $stalePrefabStatus = [regex]::Replace(
        $roadmapSource,
        '(?m)^>\s*\*\*Status:\*\*\s*Available[^\r\n]*$',
        '> **Status:** Foundation; the authored Prefab workflow remains incomplete.',
        1)
    if ($stalePrefabStatus -eq $roadmapSource) {
        throw "Documentation truth regression could not locate the bounded Prefab status."
    }
    [System.IO.File]::WriteAllText($roadmapPath, $stalePrefabStatus)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a Prefab status that disagrees with the capability status authority."
    }
    Write-Host "[pass] Documentation truth regression rejected a stale Prefab status claim."
    [System.IO.File]::WriteAllText($roadmapPath, $roadmapSource)

    $uiPath = Join-Path $probeRoot "docs\ui.md"
    $uiSource = [System.IO.File]::ReadAllText($uiPath)
    $staleSlotScale = $uiSource.Replace(
        "### Named layout slots",
        "### Named layout slots$([Environment]::NewLine)Each named layout slot stores UI scale.")
    if ($staleSlotScale -eq $uiSource) {
        throw "Documentation truth regression could not prepare the named-layout scale probe."
    }
    [System.IO.File]::WriteAllText($uiPath, $staleSlotScale)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted workspace layout ownership of global Editor UI Scale."
    }
    Write-Host "[pass] Documentation truth regression rejected workspace-owned Editor UI Scale."
    [System.IO.File]::WriteAllText($uiPath, $uiSource)

    $matrixPath = Join-Path $probeRoot "docs\engine-completion-matrix.md"
    $matrixSource = [System.IO.File]::ReadAllText($matrixPath)
    $staleFontStatus = [regex]::Replace(
        $matrixSource,
        'Atkinson Hyperlegible Regular is the bundled product default\.',
        'Atkinson Hyperlegible Regular remains only a candidate.',
        1)
    if ($staleFontStatus -eq $matrixSource) {
        throw "Documentation truth regression could not prepare the editor-font status probe."
    }
    [System.IO.File]::WriteAllText($matrixPath, $staleFontStatus)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a candidate-only status for the bundled product font."
    }
    Write-Host "[pass] Documentation truth regression rejected stale bundled-font status."
    [System.IO.File]::WriteAllText($matrixPath, $matrixSource)

    $misplacedIconPolicy = $matrixSource + [Environment]::NewLine +
        "## Modeling-readiness gate for editor icons" + [Environment]::NewLine +
        "Production editor icons must be authored with Henka's modeling tools." + [Environment]::NewLine
    [System.IO.File]::WriteAllText($matrixPath, $misplacedIconPolicy)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted implementation policy inside the public capability inventory."
    }
    Write-Host "[pass] Documentation truth rejected detailed editor-icon policy in the capability matrix."
    [System.IO.File]::WriteAllText($matrixPath, $matrixSource)

    $codingStandardPath = Join-Path $probeRoot "docs\coding-standard.md"
    $codingStandardSource = [System.IO.File]::ReadAllText($codingStandardPath)
    $codingStandardWithoutIconPolicy = [regex]::Replace(
        $codingStandardSource,
        '(?ms)^##\s+Production editor icons\s*\r?\n.*?(?=^##\s|\z)',
        '',
        1)
    if ($codingStandardWithoutIconPolicy -eq $codingStandardSource) {
        throw "Documentation truth regression could not locate the contributor editor-icon policy."
    }
    [System.IO.File]::WriteAllText($codingStandardPath, $codingStandardWithoutIconPolicy)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted missing contributor editor-icon authorship requirements."
    }
    Write-Host "[pass] Documentation truth rejected missing contributor editor-icon authorship requirements."
    [System.IO.File]::WriteAllText($codingStandardPath, $codingStandardSource)

    [System.IO.File]::AppendAllText(
        $readmePath,
        [Environment]::NewLine + "- Scene/project serialization, hierarchy authoring remain open." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a blanket hierarchy claim that omits existing bounded parenting."
    }
    Write-Host "[pass] Documentation truth rejected an obsolete blanket hierarchy gap."
    [System.IO.File]::WriteAllText($readmePath, $readmeSource)

    [System.IO.File]::AppendAllText(
        $roadmapPath,
        [Environment]::NewLine + "Explicit serialized per-instance override metadata is not yet available." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale Prefab override claim that contradicts HSCN v17."
    }
    Write-Host "[pass] Documentation truth rejected stale Prefab override wording."
    [System.IO.File]::WriteAllText($roadmapPath, $roadmapSource)

    [System.IO.File]::AppendAllText(
        $uiPath,
        [Environment]::NewLine + "- Full hierarchy editing is planned separately." + [Environment]::NewLine +
            "- Full asset browsing is planned separately." + [Environment]::NewLine +
            "- Complete numeric property editing is planned separately." + [Environment]::NewLine +
            "- Scene Hierarchy / Parenting;" + [Environment]::NewLine + "- Prefabs;" + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted future-only UI wording for workflows that already have bounded implementations."
    }
    Write-Host "[pass] Documentation truth rejected obsolete future-only UI workflow wording."
    [System.IO.File]::WriteAllText($uiPath, $uiSource)

    $actionApiPath = Join-Path $probeRoot "docs\action-api.md"
    $actionApiSource = [System.IO.File]::ReadAllText($actionApiPath)
    [System.IO.File]::AppendAllText(
        $actionApiPath,
        [Environment]::NewLine + "The Sandbox outliner, hierarchy history, and broader editor authoring surface remain separate work." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted an obsolete claim that the hierarchy projection and history are absent."
    }
    Write-Host "[pass] Documentation truth rejected obsolete hierarchy projection wording."
    [System.IO.File]::WriteAllText($actionApiPath, $actionApiSource)

    $modelLoadingPath = Join-Path $probeRoot "docs\model-loading.md"
    $modelLoadingSource = [System.IO.File]::ReadAllText($modelLoadingPath)
    [System.IO.File]::AppendAllText(
        $modelLoadingPath,
        [Environment]::NewLine + "- editor hierarchy authoring;" + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted an unqualified hierarchy limitation for model import."
    }
    Write-Host "[pass] Documentation truth rejected an unqualified imported-hierarchy limitation."
    [System.IO.File]::WriteAllText($modelLoadingPath, $modelLoadingSource)

    $sceneArchitecturePath = Join-Path $probeRoot "docs\scene-authoring-architecture.md"
    $sceneArchitectureSource = [System.IO.File]::ReadAllText($sceneArchitecturePath)
    $staleSceneVersion = [regex]::Replace(
        $sceneArchitectureSource,
        'HSCN v17',
        'HSCN v16',
        1)
    if ($staleSceneVersion -eq $sceneArchitectureSource) {
        throw "Documentation truth regression could not prepare the scene-format version probe."
    }
    [System.IO.File]::WriteAllText($sceneArchitecturePath, $staleSceneVersion)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale HSCN current-format claim."
    }
    Write-Host "[pass] Documentation truth regression rejected stale HSCN writer version."
    [System.IO.File]::WriteAllText($sceneArchitecturePath, $sceneArchitectureSource)

    $platformSupportPath = Join-Path $probeRoot "docs\platform-support.md"
    $platformSupportSource = [System.IO.File]::ReadAllText($platformSupportPath)
    [System.IO.File]::AppendAllText(
        $platformSupportPath,
        [Environment]::NewLine + "Windows CI currently uses a deterministic packaged startup smoke test with no mouse, keyboard, or desktop-focus dependency." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted the obsolete startup-only hosted-validation claim."
    }
    Write-Host "[pass] Documentation truth regression rejected obsolete hosted-validation scope."
    [System.IO.File]::WriteAllText($platformSupportPath, $platformSupportSource)

    $staleManualClaim = [System.IO.File]::ReadAllText(
        (Join-Path $probeRoot "docs\qa\sandbox3d-manual-checklist.md"))
    $manualPath = Join-Path $probeRoot "docs\qa\sandbox3d-manual-checklist.md"
    [System.IO.File]::AppendAllText(
        $manualPath,
        [Environment]::NewLine + "- The UI overlay is still intentionally small and is not an editor." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted the obsolete claim that Sandbox is not an editor."
    }
    Write-Host "[pass] Documentation truth regression rejected obsolete non-editor wording."
    [System.IO.File]::WriteAllText($manualPath, $staleManualClaim)

    $helpPath = Join-Path $probeRoot "docs\help\sandbox3d.md"
    $helpSource = [System.IO.File]::ReadAllText($helpPath)
    [System.IO.File]::AppendAllText(
        $helpPath,
        [Environment]::NewLine + "Editor hierarchy authoring remains future work." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a blanket claim that omits the current bounded hierarchy workflow."
    }
    Write-Host "[pass] Documentation truth regression rejected a blanket hierarchy capability claim."
    [System.IO.File]::WriteAllText($helpPath, $helpSource)

    [System.IO.File]::AppendAllText(
        $helpPath,
        [Environment]::NewLine + "Complete scene/project authoring remains future work." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a blanket claim that omits the bounded scene workflow."
    }
    Write-Host "[pass] Documentation truth regression rejected a blanket scene-workflow capability claim."
    [System.IO.File]::WriteAllText($helpPath, $helpSource)

    $statusProbe = [regex]::Replace(
        $readmeSource,
        '(?m)^\|\s*Renderer\s*\|\s*Available \(Unhardened\)\s*\|',
        '| Renderer | Available |',
        1)
    if ($statusProbe -eq $readmeSource) {
        throw "Documentation truth regression could not locate the Renderer status row."
    }
    [System.IO.File]::WriteAllText($readmePath, $statusProbe)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a README status escalation unsupported by the status contract."
    }
    Write-Host "[pass] Documentation truth regression rejected an unsupported capability status escalation."
    [System.IO.File]::WriteAllText($readmePath, $readmeSource)

    $modelLoadingPath = Join-Path $probeRoot "docs\model-loading.md"
    $modelLoadingSource = [System.IO.File]::ReadAllText($modelLoadingPath)
    [System.IO.File]::AppendAllText(
        $modelLoadingPath,
        [Environment]::NewLine + "[Broken anchor regression](#deliberately-missing-heading)" + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a deliberately broken Markdown heading anchor."
    }
    Write-Host "[pass] Documentation truth regression rejected a broken Markdown heading anchor."
    [System.IO.File]::WriteAllText($modelLoadingPath, $modelLoadingSource)
    if ((Invoke-TruthCheck) -ne 0) {
        throw "Documentation truth remained failed after the broken Markdown heading anchor probe was removed."
    }
    Write-Host "[pass] Documentation truth returned green after the broken Markdown heading anchor probe was removed."

    $buildingPath = Join-Path $probeRoot "docs\building.md"
    $buildingSource = [System.IO.File]::ReadAllText($buildingPath)
    [System.IO.File]::AppendAllText(
        $buildingPath,
        [Environment]::NewLine + "Legacy path: _local/builds/<checkout-key>." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a generated build path that disagrees with the shared resolver."
    }
    Write-Host "[pass] Documentation truth rejected stale generated build-root guidance."
    [System.IO.File]::WriteAllText($buildingPath, $buildingSource)

    $dedicatedServerPackagePath = Join-Path $probeRoot "scripts\package_dedicated_server_windows.ps1"
    $dedicatedServerPackageSource = [System.IO.File]::ReadAllText($dedicatedServerPackagePath)
    $stalePackageMetadata = $dedicatedServerPackageSource.Replace(
        'Source executable (relative to build root): examples/dedicated_server/$Configuration/henka_dedicated_server.exe',
        'Source executable: _local/builds/<checkout-key>/examples/dedicated_server/$Configuration/henka_dedicated_server.exe')
    if ($stalePackageMetadata -eq $dedicatedServerPackageSource) {
        throw "Documentation truth regression could not locate the dedicated-server provenance path."
    }
    [System.IO.File]::WriteAllText($dedicatedServerPackagePath, $stalePackageMetadata)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted stale dedicated-server package provenance."
    }
    Write-Host "[pass] Documentation truth rejected stale package provenance paths."
    [System.IO.File]::WriteAllText($dedicatedServerPackagePath, $dedicatedServerPackageSource)

    $securityPath = Join-Path $probeRoot "SECURITY.md"
    $securitySource = [System.IO.File]::ReadAllText($securityPath)
    [System.IO.File]::AppendAllText(
        $securityPath,
        [Environment]::NewLine + "Henka has no network-backed runtime behavior." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a false blanket denial of existing network behavior."
    }
    Write-Host "[pass] Documentation truth rejected an inaccurate network-capability denial."
    [System.IO.File]::WriteAllText($securityPath, $securitySource)

    $helpPath = Join-Path $probeRoot "docs\help\sandbox3d.md"
    $helpSource = [System.IO.File]::ReadAllText($helpPath)
    $withoutSceneCatalog = [regex]::Replace(
        $helpSource,
        '(?ms)^The Open Scene form lists project-relative `\.hscene` files in a bounded,\r?\n.*?^the catalog is unavailable, enter a project-relative `\.hscene` path manually\.\r?\n',
        "",
        1)
    if ($withoutSceneCatalog -eq $helpSource) {
        throw "Documentation truth regression could not locate the Open Scene catalog guidance."
    }
    [System.IO.File]::WriteAllText($helpPath, $withoutSceneCatalog)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted missing Open Scene catalog guidance."
    }
    Write-Host "[pass] Documentation truth rejected missing Open Scene catalog guidance."
    [System.IO.File]::WriteAllText($helpPath, $helpSource)

    [System.IO.File]::AppendAllText(
        $helpPath,
        [Environment]::NewLine + "Open Scene accepts only a manually entered project-relative path." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale path-only description of Open Scene."
    }
    Write-Host "[pass] Documentation truth rejected stale path-only Open Scene guidance."
    [System.IO.File]::WriteAllText($helpPath, $helpSource)

    $currentCapabilitiesPath = Join-Path $probeRoot "docs\current-capabilities.md"
    $currentCapabilitiesSource = [System.IO.File]::ReadAllText($currentCapabilitiesPath)
    [System.IO.File]::AppendAllText(
        $currentCapabilitiesPath,
        [Environment]::NewLine + "HSCN v16 and earlier files use the legacy default." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted an incomplete legacy HSCN version range."
    }
    Write-Host "[pass] Documentation truth rejected an incomplete legacy HSCN version range."
    [System.IO.File]::WriteAllText($currentCapabilitiesPath, $currentCapabilitiesSource)

    $realismPath = Join-Path $probeRoot "docs\realism.md"
    $realismSource = [System.IO.File]::ReadAllText($realismPath)
    [System.IO.File]::AppendAllText(
        $realismPath,
        [Environment]::NewLine + "This regression remains protected by executable negative controls." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted internal defect-triage status in public realism documentation."
    }
    Write-Host "[pass] Documentation truth rejected internal defect-triage wording."
    [System.IO.File]::WriteAllText($realismPath, $realismSource)

    $authoringMeshSourcePath = Join-Path $probeRoot "engine\src\mesh\authoring_mesh.c"
    $authoringMeshSource = [System.IO.File]::ReadAllText($authoringMeshSourcePath)
    $writerVersionMatch = [regex]::Match(
        $authoringMeshSource,
        '(?m)^\s*#define\s+HENKA_AUTHORING_MESH_FILE_VERSION\s+(\d+)U\b')
    if (-not $writerVersionMatch.Success) {
        throw "Documentation truth regression could not determine the copied writer version."
    }
    $writerVersion = [int]$writerVersionMatch.Groups[1].Value
    $staleVersion = if ($writerVersion -gt 1) { $writerVersion - 1 } else { $writerVersion + 1 }
    [System.IO.File]::AppendAllText(
        $authoringMeshPath,
        [Environment]::NewLine + "The current writer emits HAMS v$staleVersion." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale current-writer version claim."
    }
    Write-Host "[pass] Documentation truth regression rejected a stale current-writer claim."
} finally {
    if (Test-Path -LiteralPath $probeRoot) {
        Remove-Item -LiteralPath $probeRoot -Recurse -Force
    }
}

exit 0
