# Audio Foundation

Henka's first Audio slice is a renderer-independent, headless-safe runtime
foundation in `engine/include/henka/audio.h`.

## Available foundation

- Resident PCM WAV clips are loaded through the same confined project-relative
  path boundary used by other file-backed runtime data.
- A bounded PCM WAV stream API is available for long-form content. It validates
  the file metadata without decoding the full payload, keeps a file-backed
  handle, reads caller-owned float frames in bounded chunks, and can drive
  generation-checked voices or object-attached emitters without loading the
  entire source into resident sample memory. Stream reads are single-owner and
  caller-synchronized, matching the current deterministic mixer contract.
- The asset manager caches resident WAV clips and metadata-first WAV streams by
  canonical path. A path may expose both borrowed payload forms through one
  manager-owned asset identity; resident clips can be reloaded transactionally
  in place, while streamed payloads retain bounded file-backed ownership.
  Malformed or unreadable resident replacements leave the prior payload and
  metadata live.
- A fixed-capacity voice pool uses generation-checked voice IDs. Voices bind to
  borrowed `henka_scene` and `henka_entity` objects and read the live entity
  transform while mixing.
- The bounded bus layout is Master, Music, SFX, Dialogue, Ambience, and UI.
  The current mixer supports per-bus gains, listener orientation, distance
  attenuation, stereo panning, looping, pitch, and deterministic interleaved
  stereo float-PCM output. Resident voices also support bounded pause, resume,
  restart, seek, gain, and pitch controls with generation-checked IDs.
- Destroyed or stale scene entities are rejected before they contribute audio.
  Scene and clip owners must stop dependent voices before destroying those
  borrowed objects.
- Scene Document v5 stores a bounded, value-only emitter configuration on the
  real authored object record and a value-only listener, including the
  resident/streamed storage choice. v1 through v3 documents load with the
  listener at its default, while v4 loads its authored listener and defaults
  storage to resident; loading a legacy document does not rewrite it.

The headless-safe mixer is the deterministic production-output boundary for
the renderer-independent portion of this slice. A client-only, caller-pumped
SDL3 output owner now opens a playback stream, accepts bounded stereo
float-PCM frames, and reports device/queue diagnostics. It can transactionally
reopen its SDL stream after device loss and retry one failed queue submission;
successful recovery clears only queued device data. It does not create a
background mixer thread; the caller owns scene and Audio synchronization.

## Current development

The Sandbox Play session has a bounded runtime-emitter instantiation path when
the caller supplies an Audio system: it reads the persisted v5 emitter
configuration and binds each emitter to the same real Scene entity used by
Play. The normal graphical Sandbox now owns a client Audio system, maps its
production camera to the listener, and caller-pumps the SDL3 output boundary.
Play applies the persisted Scene Document listener before creating runtime
emitters; the graphical camera remains the live listener source during normal
interactive runtime.
Play pause and resume now propagate to its live emitter voices without
advancing their source positions. The next Audio slices must connect it to
  broader device-lifecycle policy. A packaged `--audio-smoke-test` now loads
  the repository-owned `assets/audio/henka_audio_fixture.wav` through the real
  asset manager in both resident and metadata-first streamed modes. Each mode
  attaches the payload to a real scene entity, mixes it through a live emitter,
  and reaches the SDL output boundary. This is deterministic production-path
  coverage for one packaged PCM WAV fixture in both storage modes, not broad
  long-form or packaged-content coverage.
Audio integrations preserve the renderer-free dedicated-server path. Integration
coverage includes real imported or authored objects.

The Sandbox Object Details panel provides an Audio group for authored scene
objects. It edits the persisted clip path, enabled, looping, spatial, and
resident/streamed storage settings through the existing Game Authoring
transaction. Preview and Stop
Preview use the reusable Audio runtime helper: the asset manager owns the
resident clip, the scene owns the entity, and the Audio runtime owns the
temporary emitter. Preview replacement is transactional and failed asset
loads leave the active preview unchanged.

The shared Script Host exposes typed `Audio.Stop(entity)`,
`Audio.Restart(entity)`, and `Audio.IsPlaying(entity)` bindings to both Lua and
HenkaScript in Play. They operate on the persisted object-to-emitter mapping;
missing or stale emitter bindings fail closed, while `IsPlaying` reports false.

## Future work

Packaged long-form streamed-content coverage, broader decoder coverage, mixer
effects, broader hot reload policy, and expanded packaged-content coverage,
device-loss notification/hot-plug policy, and broader spatial/occlusion features
remain future work. The current script bindings cover only the three typed emitter
controls above; broader Audio scripting remains unfinished. None of those gaps
are hidden by the current Foundation status.
