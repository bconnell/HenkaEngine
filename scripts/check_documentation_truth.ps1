Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$findings = New-Object System.Collections.Generic.List[string]

function Add-Finding {
    param([string]$Message)
    $findings.Add($Message)
}

function Get-RepositoryText {
    param([string]$RelativePath)

    $path = Join-Path $repoRoot ($RelativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-Finding "${RelativePath}: file is missing"
        return ""
    }
    return [System.IO.File]::ReadAllText($path)
}

$readme = Get-RepositoryText "README.md"
$capabilities = Get-RepositoryText "docs/current-capabilities.md"
$roadmap = Get-RepositoryText "docs/roadmap.md"
$editorUi = Get-RepositoryText "docs/ui.md"
$editorControls = Get-RepositoryText "docs/editor-controls.md"
$mcpQaHarness = Get-RepositoryText "docs/mcp-qa-harness.md"
$completionMatrix = Get-RepositoryText "docs/engine-completion-matrix.md"
$codingStandard = Get-RepositoryText "docs/coding-standard.md"
$actionApi = Get-RepositoryText "docs/action-api.md"
$modelLoading = Get-RepositoryText "docs/model-loading.md"
$sceneArchitecture = Get-RepositoryText "docs/scene-authoring-architecture.md"
$audioDocumentation = Get-RepositoryText "docs/audio.md"
$runtimeFoundations = Get-RepositoryText "docs/runtime-foundations.md"
$platformSupport = Get-RepositoryText "docs/platform-support.md"
$sceneDocumentHeader = Get-RepositoryText "engine/include/henka/scene_document.h"
$sceneDocumentSource = Get-RepositoryText "engine/src/core/scene_document.c"
$authoringMesh = Get-RepositoryText "docs/authoring-mesh.md"
$componentIdentities = Get-RepositoryText "docs/authoring-component-identities.md"
$authoringMeshSource = Get-RepositoryText "engine/src/mesh/authoring_mesh.c"
$scripting = Get-RepositoryText "docs/scripting-foundation.md"
$externalProjects = Get-RepositoryText "docs/external-game-projects.md"
$externalTemplate = Get-RepositoryText "templates/external_game_minimal/src/main.c"
$sandboxMain = Get-RepositoryText "examples/sandbox3d/main.c"
$sandboxHelp = Get-RepositoryText "docs/help/sandbox3d.md"
$sandboxQa = Get-RepositoryText "docs/qa/sandbox3d-manual-checklist.md"
$showcaseAssets = Get-RepositoryText "docs/showcase-assets.md"
$capabilityStatuses = Get-RepositoryText "docs/capability-statuses.tsv"

$allowedCapabilityStatuses = @(
    "Foundation",
    "In Progress",
    "Available (Unhardened)",
    "Available",
    "Planned"
)
$expectedCapabilityAreas = @(
    "Core runtime",
    "Renderer",
    "Scene and camera",
    "Editor workspace",
    "Modeling and authoring",
    "Assets and materials",
    "Terrain and world",
    "Physics",
    "2.5D",
    "Networking/server",
    "External projects",
    "Game authoring",
    "Prefabs",
    "2D",
    "Audio",
    "Scripting/behaviors"
)
$statusRecords = New-Object System.Collections.Generic.List[object]
foreach ($rawLine in @($capabilityStatuses -split "`r?`n")) {
    $line = $rawLine.Trim()
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
        continue
    }
    $parts = $line -split '\|', 3
    if ($parts.Count -ne 3) {
        Add-Finding "docs/capability-statuses.tsv: malformed status record '$line'"
        continue
    }
    $area = $parts[0].Trim()
    $status = $parts[1].Trim()
    $authoritySection = $parts[2].Trim()
    if ([string]::IsNullOrWhiteSpace($area) -or
        $status -notin $allowedCapabilityStatuses -or
        [string]::IsNullOrWhiteSpace($authoritySection)) {
        Add-Finding "docs/capability-statuses.tsv: invalid status record '$line'"
        continue
    }
    $statusRecords.Add([pscustomobject]@{
        Area = $area
        Status = $status
        AuthoritySection = $authoritySection
    })
}

if ($statusRecords.Count -ne $expectedCapabilityAreas.Count) {
    Add-Finding "docs/capability-statuses.tsv: expected $($expectedCapabilityAreas.Count) capability rows, found $($statusRecords.Count)"
}
foreach ($expectedArea in $expectedCapabilityAreas) {
    $matches = @($statusRecords | Where-Object { $_.Area -eq $expectedArea })
    if ($matches.Count -ne 1) {
        Add-Finding "docs/capability-statuses.tsv: expected exactly one row for '$expectedArea'"
    }
}
foreach ($record in $statusRecords) {
    if ($record.Area -notin $expectedCapabilityAreas) {
        Add-Finding "docs/capability-statuses.tsv: unknown capability area '$($record.Area)'"
    }
    $authorityPattern = '(?ms)^' + [regex]::Escape($record.AuthoritySection) + '\s*$\r?\n(.*?)(?=^##\s|\z)'
    $authorityMatch = [regex]::Match($capabilities, $authorityPattern)
    if (-not $authorityMatch.Success) {
        Add-Finding "docs/current-capabilities.md: missing authority section '$($record.AuthoritySection)' for '$($record.Area)'"
        continue
    }
    if ($record.Status -eq "Available") {
        if ($authorityMatch.Groups[1].Value -match '(?i)\b(?:not\s+(?:a\s+)?complete|remain(?:s)?\s+(?:unfinished|incomplete|open)|future\s+work|foundation\s*(?:only|not))\b') {
            Add-Finding "docs/current-capabilities.md: authority section '$($record.AuthoritySection)' contains an incomplete-category claim for '$($record.Status)'"
        }
    }
}

$prefabStatusRecord = @($statusRecords | Where-Object { $_.Area -eq "Prefabs" })
$prefabRoadmapSection = [regex]::Match(
    $roadmap,
    '(?ms)^##\s+Prefabs\s*/\s*Reusable Scene Objects\s*\r?\n(.*?)(?=^##\s|\z)')
if ($prefabStatusRecord.Count -ne 1 -or -not $prefabRoadmapSection.Success) {
    Add-Finding "docs/roadmap.md: Prefab status section could not be matched to the capability status authority"
} else {
    $prefabRoadmapStatus = [regex]::Match(
        $prefabRoadmapSection.Groups[1].Value,
        '(?im)^>\s*\*\*Status:\*\*\s*([^\r\n]+)')
    $expectedPrefabStatus = [string]$prefabStatusRecord[0].Status
    if (-not $prefabRoadmapStatus.Success -or
        $prefabRoadmapStatus.Groups[1].Value.Trim() -notmatch (
            '^' + [regex]::Escape($expectedPrefabStatus) + '(?:\b|\s|$)')) {
        Add-Finding "docs/roadmap.md: Prefab status disagrees with docs/capability-statuses.tsv ('$expectedPrefabStatus')"
    }
}

$namedLayoutSlots = [regex]::Match(
    $editorUi,
    '(?ms)^###\s+Named layout slots\s*\r?\n(.*?)(?=^#{1,3}\s|\z)')
if ($editorUi -notmatch '(?i)Editor UI Scale is a separate global editor preference' -or
    $editorUi -notmatch '(?is)ui\.workspace\.custom_layout\.ui_scale.{0,180}one-time compatibility migration' -or
    $editorUi -notmatch '(?i)ui\.editor\.scale_percent' -or
    -not $namedLayoutSlots.Success) {
    Add-Finding "docs/ui.md: global Editor UI Scale ownership and legacy migration must be documented separately from workspace layouts"
} elseif ($namedLayoutSlots.Groups[1].Value -match '(?i)(?:named\s+)?layout\s+slots?.{0,100}(?:store|persist|own|override).{0,60}(?:Editor\s+)?UI\s+Scale|(?:store|persist|own|override).{0,60}(?:Editor\s+)?UI\s+Scale.{0,100}(?:named\s+)?layout\s+slots?') {
    Add-Finding "docs/ui.md: named workspace layout slots must not store or override global Editor UI Scale"
}

$editorIconPolicy = [regex]::Match(
    $codingStandard,
    '(?ms)^##\s+Production editor icons\s*\r?\n(.*?)(?=^##\s|\z)')
if (-not $editorIconPolicy.Success -or
    $editorIconPolicy.Groups[1].Value -notmatch '(?is)Henka.*modeling.*tools.{0,300}save/reload.{0,300}packaging.{0,300}without color alone') {
    Add-Finding "docs/coding-standard.md: production editor-icon authorship and validation rules are missing"
}
if ($completionMatrix -match '(?im)^##\s+Modeling-readiness gate for editor icons\s*$') {
    Add-Finding "docs/engine-completion-matrix.md: detailed editor-icon implementation policy belongs in contributor standards, not the capability inventory"
}
if ($readme -match '(?i)Scene/project serialization,\s*hierarchy authoring') {
    Add-Finding "README.md: hierarchy authoring exists within the bounded parent/unparent/reparent workflow; describe the broader gap precisely"
}
if ($roadmap -match '(?i)explicit serialized per-instance override metadata is not yet available') {
    Add-Finding "docs/roadmap.md: HSCN v17 persists supported non-root prefab local-transform override metadata"
}
if ($editorUi -match '(?im)^-\s*(?:Full hierarchy editing is planned separately|Full asset browsing is planned separately|Complete numeric property editing is planned separately|Scene Hierarchy / Parenting;|Prefabs;)') {
    Add-Finding "docs/ui.md: describe current bounded hierarchy, Asset Browser, World Transform Inspector, and Prefab workflows before broader planned work"
}
if ($actionApi -match '(?is)The Sandbox\s+outliner,\s*hierarchy history,\s*and broader editor authoring surface remain\s+separate work') {
    Add-Finding "docs/action-api.md: bounded Scene Objects hierarchy projection and Game Authoring history are available"
}
if ($modelLoading -match '(?im)^\s*(?:-\s*editor hierarchy authoring\s*;?|Editor hierarchy authoring remains future work\.?)\s*$') {
    Add-Finding "docs/model-loading.md: distinguish imported model-node hierarchy support from existing scene-object parenting controls"
}

$workspaceMatrixRow = [regex]::Match(
    $completionMatrix,
    '(?m)^\|\s*Editor workspace and UI\s*\|[^\r\n]*$')
if (-not $workspaceMatrixRow.Success -or
    $workspaceMatrixRow.Value -notmatch '(?i)Atkinson Hyperlegible Regular is the bundled product default') {
    Add-Finding "docs/engine-completion-matrix.md: bundled Atkinson Hyperlegible Regular must be described as the product default"
}

if ($sandboxQa -match '(?i)UI overlay is still intentionally small and is not an editor') {
    Add-Finding "docs/qa/sandbox3d-manual-checklist.md: obsolete claim that the Sandbox UI is not an editor"
}
if ($platformSupport -match '(?i)Windows CI currently uses a deterministic packaged startup smoke test with no mouse, keyboard, or desktop-focus dependency') {
    Add-Finding "docs/platform-support.md: hosted Windows validation is incorrectly described as startup-only"
}
foreach ($workflowTerm in @("New Scene", "Save As", "Open Scene", "Reload", "Save / Discard / Cancel")) {
    if ($sandboxHelp -notmatch [regex]::Escape($workflowTerm)) {
        Add-Finding "docs/help/sandbox3d.md: current packaged Game Authoring workflow is missing '$workflowTerm'"
    }
}

$transformInspectorSection = [regex]::Match(
    $editorControls,
    '(?ms)^## Numeric World Transform Inspector\s*\r?\n(.*?)(?=^##\s|\z)')
if (-not $transformInspectorSection.Success -or
    $transformInspectorSection.Groups[1].Value -notmatch '(?i)pending' -or
    $transformInspectorSection.Groups[1].Value -notmatch '(?i)Apply' -or
    $transformInspectorSection.Groups[1].Value -notmatch '(?i)Revert' -or
    $transformInspectorSection.Groups[1].Value -notmatch '(?i)Game Authoring' -or
    $transformInspectorSection.Groups[1].Value -notmatch '(?i)Play') {
    Add-Finding "docs/editor-controls.md: numeric World Transform Inspector must explain pending fields, Apply/Revert, Game Authoring history, and its Play restriction"
}

$visualEvidenceSection = [regex]::Match(
    $mcpQaHarness,
    '(?ms)^## Visual evidence boundary\s*\r?\n(.*?)(?=^##\s|\z)')
if (-not $visualEvidenceSection.Success -or
    $visualEvidenceSection.Groups[1].Value -notmatch '(?i)MCP adapter' -or
    $visualEvidenceSection.Groups[1].Value -notmatch '(?i)application-owned framebuffer' -or
    $visualEvidenceSection.Groups[1].Value -notmatch '(?i)capture_visual_evidence_windows\.ps1' -or
    $visualEvidenceSection.Groups[1].Value -notmatch '(?i)visual acceptance' -or
    $visualEvidenceSection.Groups[1].Value -notmatch '(?i)does not add framebuffer-capture, save, or reload\s+operations to the adapter') {
    Add-Finding "docs/mcp-qa-harness.md: distinguish MCP capture scope from packaged application-owned framebuffer evidence and visual acceptance"
}

$sceneDocumentVersionMatch = [regex]::Match(
    $sceneDocumentHeader,
    '(?m)^#define\s+HENKA_SCENE_DOCUMENT_FORMAT_VERSION\s+UINT32_C\((\d+)\)')
if (-not $sceneDocumentVersionMatch.Success) {
    Add-Finding "engine/include/henka/scene_document.h: current HSCN writer version could not be determined"
} else {
    $currentSceneDocumentVersion = [int]$sceneDocumentVersionMatch.Groups[1].Value
    $currentSceneDocumentClaim = "HSCN v$currentSceneDocumentVersion"
    foreach ($document in @(
        [pscustomobject]@{ Path = "docs/current-capabilities.md"; Text = $capabilities },
        [pscustomobject]@{ Path = "docs/scene-authoring-architecture.md"; Text = $sceneArchitecture }
    )) {
        if ($document.Text -notmatch [regex]::Escape($currentSceneDocumentClaim)) {
            Add-Finding "$($document.Path): current HSCN format '$currentSceneDocumentClaim' is not documented"
        }
    }
    $readerRangeClaim = "reader explicitly accepts HSCN v1 through v$currentSceneDocumentVersion"
    if ($sceneArchitecture -notmatch [regex]::Escape($readerRangeClaim)) {
        Add-Finding "docs/scene-authoring-architecture.md: accepted HSCN reader range must match the versioned implementation"
    }

    $audioEmitterVersionMatch = [regex]::Match(
        $sceneDocumentSource,
        '(?s)format_version\s*>=\s*HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V(\d+)\s*\)\s*\{\s*uint32_t\s+audio_bus;')
    $audioListenerVersionMatch = [regex]::Match(
        $sceneDocumentSource,
        '(?s)format_version\s*>=\s*HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V(\d+)\s*&&\s*\(\s*!henka_scene_document_reader_float\(\s*&reader\s*,\s*&candidate->audio_listener\.position\.x')
    $renderResourcesVersionMatch = [regex]::Match(
        $sceneDocumentSource,
        '(?s)format_version\s*>=\s*HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V(\d+)\s*&&\s*!henka_scene_document_decode_render_resources')
    $audioMatrixMatch = [regex]::Match(
        $completionMatrix,
        '(?m)^\|\s*Audio\s*\|[^\r\n]*$')
    $audioMatrixText = if ($audioMatrixMatch.Success) { $audioMatrixMatch.Value } else { "" }
    if (-not $audioEmitterVersionMatch.Success) {
        Add-Finding "engine/src/core/scene_document.c: Audio emitter introduction version could not be determined"
    }
    if (-not $audioListenerVersionMatch.Success) {
        Add-Finding "engine/src/core/scene_document.c: Audio listener introduction version could not be determined"
    }
    if (-not $renderResourcesVersionMatch.Success) {
        Add-Finding "engine/src/core/scene_document.c: render-resource introduction version could not be determined"
    }

    if ($audioEmitterVersionMatch.Success -and $audioListenerVersionMatch.Success) {
        $audioEmitterVersion = [int]$audioEmitterVersionMatch.Groups[1].Value
        $audioListenerVersion = [int]$audioListenerVersionMatch.Groups[1].Value
        $audioHistoryPattern = '(?is)Historically,\s*HSCN v' + $audioEmitterVersion +
            ' introduced per-object audio emitter settings and HSCN\s*v' + $audioListenerVersion +
            ' introduced the document-level listener\.\s*The current writer emits HSCN\s*v' +
            $currentSceneDocumentVersion + '\.'
        if ($audioDocumentation -notmatch $audioHistoryPattern) {
            Add-Finding "docs/audio.md: historical emitter/listener format milestones and current writer version must match Scene Document source"
        }

        $audioMatrixPattern = '(?i)Current HSCN v' + $currentSceneDocumentVersion +
            ' writer; Audio emitter fields introduced in v' + $audioEmitterVersion +
            ' and listener in v' + $audioListenerVersion + '(?=[;|.])'
        if (-not $audioMatrixMatch.Success -or $audioMatrixMatch.Value -notmatch $audioMatrixPattern) {
            Add-Finding "docs/engine-completion-matrix.md: Audio persistence must distinguish historical field introduction from the current HSCN writer"
        }
    }

    if ($renderResourcesVersionMatch.Success) {
        $renderResourcesVersion = [int]$renderResourcesVersionMatch.Groups[1].Value
        $renderResourcesHistoryPattern = '(?is)Historically,\s*HSCN v' + $renderResourcesVersion +
            ' introduced persisted value-owned light descriptors and\s+local reflection-probe volumes\.\s*' +
            'The current writer emits HSCN\s*v' + $currentSceneDocumentVersion + '\.'
        if ($runtimeFoundations -notmatch $renderResourcesHistoryPattern) {
            Add-Finding "docs/runtime-foundations.md: render-resource format history and current writer version must match Scene Document source"
        }
    }

    $staleCurrentWriterPattern = '(?i)\bcurrent\b.{0,100}\b(?:HSCN|Scene Document)\s+v(\d+)\b'
    foreach ($document in @(
        [pscustomobject]@{ Path = "docs/audio.md"; Text = $audioDocumentation },
        [pscustomobject]@{ Path = "docs/engine-completion-matrix.md"; Text = $audioMatrixText },
        [pscustomobject]@{ Path = "docs/runtime-foundations.md"; Text = $runtimeFoundations }
    )) {
        foreach ($writerMatch in [regex]::Matches($document.Text, $staleCurrentWriterPattern)) {
            if ([int]$writerMatch.Groups[1].Value -ne $currentSceneDocumentVersion) {
                Add-Finding "$($document.Path): claims an obsolete current HSCN writer version v$($writerMatch.Groups[1].Value)"
            }
        }
    }

    if ($sceneArchitecture -notmatch '(?im)^### Historical V1 record contract\s*$' -or
        $sceneArchitecture -match '(?i)\bFuture loaders may\b.{0,160}\b(?:validate|migrate|load)\b' -or
        $sceneArchitecture -match '(?i)\bV1 code rejects newer unsupported format versions\b') {
        Add-Finding "docs/scene-authoring-architecture.md: V1-only migration language must be identified as historical and agree with current reader support"
    }
}

$capabilityMatrixMatch = [regex]::Match($readme, '(?ms)^##\s+Capability matrix\s*\r?\n(.*?)(?=^###\s+Status meanings|^##\s|\z)')
if (-not $capabilityMatrixMatch.Success) {
    Add-Finding "README.md: capability matrix section is missing"
    $capabilityMatrix = ""
} else {
    $capabilityMatrix = $capabilityMatrixMatch.Groups[1].Value
}

$gameAuthoringRow = [regex]::Match(
    $capabilityMatrix,
    '(?m)^\|\s*Game authoring\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|')
if (-not $gameAuthoringRow.Success) {
    Add-Finding "README.md: bounded Game Authoring workflow row is missing"
} else {
    $gameAuthoringSummary = $gameAuthoringRow.Groups[2].Value
    foreach ($workflowTerm in @(
        "New Scene",
        "Save/Save As",
        "Open",
        "Reload",
        "Save/Discard/Cancel",
        "window close",
        "Play"
    )) {
        if ($gameAuthoringSummary -notmatch [regex]::Escape($workflowTerm)) {
            Add-Finding "README.md: Game Authoring summary is missing supported workflow '$workflowTerm'"
        }
    }
}

$matrixMatches = [regex]::Matches($capabilityMatrix, '(?m)^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|')
foreach ($matrixMatch in $matrixMatches) {
    $area = $matrixMatch.Groups[1].Value.Trim()
    $status = $matrixMatch.Groups[2].Value.Trim()
    if ($area -in @("Area", "---")) {
        continue
    }
    if ($area -notin $expectedCapabilityAreas) {
        Add-Finding "README.md: unknown capability matrix area '$area'"
    }
    if ($status -notin $allowedCapabilityStatuses) {
        Add-Finding "README.md: unknown capability status '$status' for '$area'"
    }
}
foreach ($record in $statusRecords) {
    $matrixPattern = '(?m)^\|\s*' + [regex]::Escape($record.Area) + '\s*\|\s*([^|]+?)\s*\|'
    $matches = [regex]::Matches($readme, $matrixPattern)
    if ($matches.Count -ne 1) {
        Add-Finding "README.md: expected exactly one capability matrix row for '$($record.Area)'"
        continue
    }
    $readmeStatus = $matches[0].Groups[1].Value.Trim()
    if ($readmeStatus -ne $record.Status) {
        Add-Finding "README.md: status for '$($record.Area)' is '$readmeStatus', expected '$($record.Status)'"
    }
}

foreach ($stalePattern in @(
    "(?i)modeling follows the production-quality 2\.5D track",
    "(?i)then integrated modeling",
    "(?i)vertex/edge deletion.{0,100}remain in progress",
    "(?i)full host API coverage.{0,100}`for` loops.{0,100}future",
    "(?is)full\s+Game/Play\s+authoring.{0,160}not\s+yet\s+available",
    "(?i)Editor hierarchy authoring remains future work",
    "(?i)Complete scene/project authoring remains future work"
)) {
    foreach ($document in @(
        [pscustomobject]@{ Path = "README.md"; Text = $readme },
        [pscustomobject]@{ Path = "docs/current-capabilities.md"; Text = $capabilities },
        [pscustomobject]@{ Path = "docs/roadmap.md"; Text = $roadmap },
        [pscustomobject]@{ Path = "docs/scripting-foundation.md"; Text = $scripting },
        [pscustomobject]@{ Path = "docs/help/sandbox3d.md"; Text = $sandboxHelp }
    )) {
        if ($document.Text -match $stalePattern) {
            Add-Finding "$($document.Path): stale capability wording matches '$stalePattern'"
        }
    }
}

foreach ($operation in @(
    "Merge Center",
    "Merge Active",
    "Merge by Distance",
    "Connect Vertices",
    "Dissolve Vertex",
    "Delete Vertex",
    "Vertex Bevel"
)) {
    if ($capabilities -notmatch [regex]::Escape($operation)) {
        Add-Finding "docs/current-capabilities.md: missing published Vertex operation '$operation'"
    }
}
if ($capabilities -notmatch "(?i)Vertex Extrude") {
    Add-Finding "docs/current-capabilities.md: Vertex Extrude must remain explicitly listed as unfinished"
}
if ($capabilities -notmatch "(?i)Edge Delete") {
    Add-Finding "docs/current-capabilities.md: Edge Delete status must remain explicit"
}
if ($roadmap -notmatch "(?i)Integrated authoring is already underway" -or
    $roadmap -notmatch "(?im)^### Implemented Foundation" -or
    $roadmap -notmatch "(?im)^### Current Development" -or
    $roadmap -notmatch "(?im)^### Future Work") {
    Add-Finding "docs/roadmap.md: integrated authoring must distinguish foundation, current development, and future work"
}

$hamsVersionMatch = [regex]::Match(
    $authoringMeshSource,
    '(?m)^\s*#define\s+HENKA_AUTHORING_MESH_FILE_VERSION\s+(\d+)U\b')
if (-not $hamsVersionMatch.Success) {
    Add-Finding "engine/src/mesh/authoring_mesh.c: current HAMS writer version could not be determined"
} else {
    $currentHamsVersion = [int]$hamsVersionMatch.Groups[1].Value
    $currentHamsClaim = "HAMS v$currentHamsVersion"
    foreach ($document in @(
        [pscustomobject]@{ Path = "docs/authoring-mesh.md"; Text = $authoringMesh },
        [pscustomobject]@{ Path = "docs/current-capabilities.md"; Text = $capabilities },
        [pscustomobject]@{ Path = "docs/authoring-component-identities.md"; Text = $componentIdentities }
    )) {
        if ($document.Text -notmatch [regex]::Escape($currentHamsClaim)) {
            Add-Finding "$($document.Path): current HAMS writer version '$currentHamsClaim' is not documented"
        }
        foreach ($staleCurrentClaim in @(
            "(?i)\bwrites\s+HAMS\s+v(?!$currentHamsVersion\b)\d+",
            "(?i)\bcurrent\s+v(?!$currentHamsVersion\b)\d+\s+format",
            "(?i)\bcurrent\s+HAMS\s+v(?!$currentHamsVersion\b)\d+",
            "(?i)\bcurrent\s+writer\s+(?:emits|is)\s+HAMS\s+v(?!$currentHamsVersion\b)\d+",
            "(?i)\bHAMS\s+v(?!$currentHamsVersion\b)\d+\s+is\s+the\s+current\s+writer"
        )) {
            if ($document.Text -match $staleCurrentClaim) {
                Add-Finding "$($document.Path): stale current HAMS claim matches '$staleCurrentClaim'"
            }
        }
    }
}

if ($readme -notmatch "(?im)^>\s*\*\*Support Henka Engine\*\*" -or
    $readme -notmatch "(?im)^##\s+Support Henka Engine" -or
    $readme -notmatch "(?i)SUPPORT\.md" -or
    $readme -notmatch "(?i)https://github\.com/sponsors/") {
    Add-Finding "README.md: sponsorship callout, dedicated section, SUPPORT.md, and direct Sponsors link are all required"
}

function Get-MarkdownHeadingAnchors {
    param([string]$Text)

    $anchors = New-Object System.Collections.Generic.List[string]
    $counts = @{}
    foreach ($headingMatch in [regex]::Matches(
            $Text,
            '(?m)^\s*#{1,6}\s+(.+?)\s*#*\s*$')) {
        $heading = $headingMatch.Groups[1].Value.Trim()
        $heading = [regex]::Replace($heading, '\[([^\]]+)\]\([^)]+\)', '$1')
        $heading = $heading.Replace('`', '')
        $heading = [regex]::Replace($heading, '<[^>]+>', '')
        $anchor = [regex]::Replace($heading.ToLowerInvariant(), '[^a-z0-9\s-]', '')
        $anchor = [regex]::Replace($anchor, '\s', '-').Trim('-')
        if ([string]::IsNullOrWhiteSpace($anchor)) {
            continue
        }
        if ($counts.ContainsKey($anchor)) {
            $counts[$anchor] += 1
            $anchor = '{0}-{1}' -f $anchor, $counts[$anchor]
        } else {
            $counts[$anchor] = 0
        }
        $anchors.Add($anchor)
    }
    return $anchors.ToArray()
}

if ($scripting -notmatch "(?i)Input\.IsActionDown.{0,180}fail-closed" -or
    $scripting -notmatch "(?i)Interaction\.Try.{0,180}unavailable" -or
    $scripting -notmatch "(?i)entity_is_valid\(entity\)" -or
    $scripting -notmatch "(?i)transform_get_position\(entity\)" -or
    $scripting -notmatch "(?i)physics_apply_impulse\(entity, impulse\)") {
    Add-Finding "docs/scripting-foundation.md: current script host availability and HenkaScript typed gameplay surface must be explicit"
}

if ($externalProjects -notmatch '(?i)package-owned `\.hks` and `\.lua` assets' -or
    $externalProjects -notmatch "(?i)HKS-to-Lua" -or
    $externalTemplate -notmatch "assets/scripts/publisher\.hks" -or
    $externalTemplate -notmatch "assets/scripts/subscriber\.lua" -or
    $externalTemplate -notmatch "HENKA_SCRIPT_LANGUAGE_HENKASCRIPT" -or
    $externalTemplate -notmatch "HENKA_SCRIPT_LANGUAGE_LUA") {
    Add-Finding "external game template: public mixed-language package proof must remain source-visible"
}

if ($sandboxMain -match '"Hidden:"') {
    Add-Finding "examples/sandbox3d/main.c: hidden-object rows must use the explicit Visibility: Hidden label"
}

foreach ($document in @(
    [pscustomobject]@{ Path = "docs/help/sandbox3d.md"; Text = $sandboxHelp },
    [pscustomobject]@{ Path = "docs/qa/sandbox3d-manual-checklist.md"; Text = $sandboxQa },
    [pscustomobject]@{ Path = "docs/showcase-assets.md"; Text = $showcaseAssets }
)) {
    if ($document.Text -match '(?i)Create Native Rocket') {
        Add-Finding "$($document.Path): removed asset-specific Create Native Rocket action is still documented"
    }
}

$trackedMarkdown = @(& (Get-HenkaGitPath) -C $repoRoot ls-files --cached --others --exclude-standard "*.md")
if ($LASTEXITCODE -ne 0) {
    throw "Unable to enumerate Markdown files."
}
foreach ($rawPath in $trackedMarkdown) {
    $relativePath = ([string]$rawPath).Trim().Replace("\", "/")
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
        continue
    }
    $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        continue
    }
    $text = [System.IO.File]::ReadAllText($absolutePath)
    foreach ($match in [regex]::Matches($text, '\[[^\]]+\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim()
        if ([string]::IsNullOrWhiteSpace($target) -or
            $target -match '^[A-Za-z][A-Za-z0-9+.-]*://') {
            continue
        }
        $targetParts = $target -split '#', 2
        $localTarget = $targetParts[0]
        $fragment = if ($targetParts.Count -eq 2) { $targetParts[1] } else { $null }
        $resolvedTarget = $absolutePath
        if (-not [string]::IsNullOrWhiteSpace($localTarget)) {
            $resolvedTarget = Join-Path (Split-Path -Parent $absolutePath) $localTarget.Replace("/", "\")
        }
        if (-not (Test-Path -LiteralPath $resolvedTarget)) {
            Add-Finding "${relativePath}: broken local Markdown link '$target'"
            continue
        }
        if ($null -ne $fragment -and
            ([string]::IsNullOrWhiteSpace($localTarget) -or
             [IO.Path]::GetExtension($resolvedTarget) -ieq '.md')) {
            try {
                $decodedFragment = [Uri]::UnescapeDataString($fragment).ToLowerInvariant()
            } catch {
                Add-Finding "${relativePath}: invalid Markdown heading anchor '$target'"
                continue
            }
            $destinationText = [System.IO.File]::ReadAllText($resolvedTarget)
            $anchors = Get-MarkdownHeadingAnchors $destinationText
            if ($anchors -notcontains $decodedFragment) {
                Add-Finding "${relativePath}: broken Markdown heading anchor '$target'"
            }
        }
    }
}

if ($findings.Count -gt 0) {
    Write-Host "Documentation truth check failed:"
    $findings | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[pass] Documentation truth and local-link check passed."
