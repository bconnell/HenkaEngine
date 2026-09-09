$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sceneStorageHeader = Get-Content -Raw (Join-Path $repoRoot 'engine/src/scene/scene_internal.h')
$runtimeHeader = Get-Content -Raw (Join-Path $repoRoot 'engine/src/runtime_internal.h')
$sceneSource = Get-Content -Raw (Join-Path $repoRoot 'engine/src/scene/scene.c')
$rendererSource = Get-Content -Raw (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c')

function Assert-Contract([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Reflection-probe revision contract failed: $message"
    }
}

Assert-Contract ($sceneStorageHeader -match '(?m)^\s*uint64_t content_revision;') `
    'henka_scene must carry a content revision separate from the camera-sensitive render revision.'
Assert-Contract ($runtimeHeader -match '(?m)^\s*#include "scene/scene_internal\.h"') `
    'runtime scene code must use the shared scene storage layout instead of a duplicate revision state.'
Assert-Contract ($sceneSource -match '(?s)static bool henka_scene_bump_render_revision\(henka_scene\* scene\).*?content_revision') `
    'content mutations must advance content_revision.'
Assert-Contract ($sceneSource -match '(?s)static bool henka_scene_bump_camera_revision\(henka_scene\* scene\).*?render_revision') `
    'camera-only mutations must have an explicit render-revision helper.'
Assert-Contract ($sceneSource -match '(?s)henka_result henka_scene_set_camera\(.*?henka_scene_bump_camera_revision\(scene\);') `
    'camera updates must not invalidate content-only reflection-probe captures.'
Assert-Contract ($sceneSource -match '(?s)henka_result henka_scene_set_entity_visible\(.*?if \(record->visible != visible\).*?henka_scene_bump_render_revision\(scene\);') `
    'repeated visibility assignments must not invalidate content-only reflection-probe captures.'
Assert-Contract ($sceneSource -match '(?s)scene->content_revision\s*=\s*1U;') `
    'new scenes must initialize content_revision.'
Assert-Contract ($sceneSource -match '(?s)clone->content_revision\s*=\s*source->content_revision;') `
    'scene clones must preserve content_revision.'
Assert-Contract ($rendererSource -match '(?s)reflection_probe_captured.*?scene->content_revision') `
    'reflection-probe capture validity must use content_revision.'
Assert-Contract (-not ($rendererSource -match '(?s)reflection_probe_captured_scene_revision.*?scene->render_revision')) `
    'reflection-probe validity must not continue using camera-sensitive render_revision.'

Write-Output 'Reflection-probe capture revision contract test passed.'
