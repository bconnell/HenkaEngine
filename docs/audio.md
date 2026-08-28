# Audio Foundation

Henka's Audio system currently provides a renderer-independent runtime core, persisted scene emitters, manager-owned resident audio assets, Play-session integration, camera/listener mapping, deterministic PCM mixing, and caller-pumped SDL3 output.

> **Status:** Foundation. Core runtime playback and scene integration are active. Streaming, broader decoding, editor authoring, scripting, device recovery, and packaged end-user proof remain in progress.

## Contents

- [Runtime core](#runtime-core)
- [Asset ownership](#asset-ownership)
- [Voices and buses](#voices-and-buses)
- [Scene emitters](#scene-emitters)
- [Play-session integration](#play-session-integration)
- [SDL3 output](#sdl3-output)
- [Current development](#current-development)
- [Future work](#future-work)

## Runtime core

The renderer-independent Audio API lives in `engine/include/henka/audio.h`.

Current runtime support includes:

- resident PCM WAV clips;
- deterministic interleaved stereo float-PCM mixing;
- listener orientation;
- distance attenuation;
- stereo panning;
- looping;
- pitch control;
- generation-checked voice IDs;
- stale-entity rejection;
- headless-safe execution.

The headless-safe mixer is the deterministic production-output boundary for the shared runtime portion of Audio.

## Asset ownership

Audio clips are now available through the shared asset manager.

`henka_assets_load_audio_clip`:

- accepts the same confined project-relative path contract used by other file-backed assets;
- canonicalizes cache identity;
- returns borrowed manager-owned clips;
- reuses an existing clip for the same canonical path;
- stores Audio asset metadata;
- destroys managed clips during asset-manager shutdown;
- grows the Audio cache through the same bounded-capacity policy used by the asset system.

Audio metadata is available through:

- `henka_assets_get_audio_metadata`;
- `henka_assets_get_audio_metadata_for_path`;
- the shared indexed asset-metadata inventory.

Current Audio metadata reports a resident PCM WAV loaded from the canonical asset path. Audio reload support is not implemented yet.

## Voices and buses

The fixed-capacity voice pool uses generation-checked IDs.

Voices can bind to real `henka_scene` and `henka_entity` objects. The mixer reads the live entity transform during mixing and rejects stale or destroyed entities before they contribute audio.

The current bus layout is:

1. Master
2. Music
3. SFX
4. Dialogue
5. Ambience
6. UI

Per-bus gain is available through the production mixer.

Scene and clip owners must stop dependent voices before destroying borrowed objects that remain in use.

## Scene emitters

Scene Document v3 stores bounded value-only Audio emitter configuration on authored object records.

Legacy behavior is explicit:

- v1 documents load with Audio disabled by default;
- v2 documents load with Audio disabled by default;
- loading a legacy document does not rewrite it.

`henka_audio_emitter_create_with_clip` creates emitters from borrowed clips, including manager-owned assets. The Audio system and borrowed clip must outlive the emitter.

## Play-session integration

The Sandbox Play session reads persisted v3 emitter configuration and binds each runtime emitter to the same real Scene entity used by Play.

The current production path can use manager-owned Audio clips during emitter creation. This keeps file-backed Audio under the same asset ownership boundary used by the wider engine.

The graphical Sandbox also owns a client Audio system and maps its production camera to the listener.

## SDL3 output

The client-only SDL3 output owner:

- opens a playback stream;
- accepts bounded stereo float-PCM frames;
- reports device diagnostics;
- reports queue diagnostics;
- is pumped by the caller.

The current output path does not create a background mixer thread. The caller owns scene and Audio synchronization.

The renderer-free dedicated-server path remains device-free.

## Current development

The active Audio campaign is closing the remaining game-audio workflow gaps around the existing runtime foundation.

Current work includes:

- broader application/device lifecycle handling;
- device-loss and recovery behavior;
- Scene Document listener authoring;
- streamed long-form assets;
- broader decoder coverage;
- asset reload and hot-reload behavior;
- editor authoring and preview controls;
- scripting/gameplay bindings;
- packaged runtime proof;
- broader real imported or authored-object integration coverage;
- lifecycle and ownership hardening across Play, scene changes, asset changes, and shutdown.

## Future work

Planned Audio maturity includes:

- long-form streaming for music and dialogue;
- broader supported audio formats;
- richer music transition and crossfade workflows;
- broader spatial features and occlusion where supported by the production architecture;
- editor-facing Audio authoring;
- scripting and gameplay APIs;
- robust device discovery, loss, and recovery;
- packaged external-project validation;
- broader performance and memory controls;
- advanced mixer/effect capabilities after the core game-audio workflow is mature.

Advanced DAW-style editing is outside the current runtime Audio campaign.
