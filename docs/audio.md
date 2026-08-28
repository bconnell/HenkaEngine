# Audio Foundation

Henka's first Audio slice is a renderer-independent, headless-safe runtime
foundation in `engine/include/henka/audio.h`.

## Available foundation

- Resident PCM WAV clips are loaded through the same confined project-relative
  path boundary used by other file-backed runtime data.
- The asset manager caches resident WAV clips by canonical path and can reload
  an existing clip transactionally in place. Borrowed clip identity remains
  stable for attached emitters; malformed or unreadable replacements leave the
  prior payload and metadata live.
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
- Scene Document v4 stores a bounded, value-only emitter configuration on the
  real authored object record and a value-only listener. v1 through v3
  documents load with the listener at its default and older Audio fields
  migrated in memory; loading a legacy document does not rewrite it.

The headless-safe mixer is the deterministic production-output boundary for
the renderer-independent portion of this slice. A client-only, caller-pumped
SDL3 output owner now opens a playback stream, accepts bounded stereo
float-PCM frames, and reports device/queue diagnostics. It can transactionally
reopen its SDL stream after device loss and retry one failed queue submission;
successful recovery clears only queued device data. It does not create a
background mixer thread; the caller owns scene and Audio synchronization.

## Current development

The Sandbox Play session has a bounded runtime-emitter instantiation path when
the caller supplies an Audio system: it reads the persisted v4 emitter
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
asset manager, attaches it to a real scene entity, mixes it through the live
emitter, and reaches the SDL output boundary. This is deterministic
production-path coverage for one resident PCM WAV fixture, not broad packaged
content coverage.
Those integrations must preserve the
renderer-free dedicated-server path and add real imported or authored-object
coverage rather than test-only entities.

The shared Script Host exposes typed `Audio.Stop(entity)`,
`Audio.Restart(entity)`, and `Audio.IsPlaying(entity)` bindings to both Lua and
HenkaScript in Play. They operate on the persisted object-to-emitter mapping;
missing or stale emitter bindings fail closed, while `IsPlaying` reports false.

## Future work

Streaming long-form assets, broader decoder coverage, mixer effects, broader hot
reload policy, editor controls, expanded packaged-content coverage,
device-loss notification/hot-plug policy, and broader spatial/occlusion features
remain future work. The current script bindings cover only the three typed emitter
controls above; broader Audio scripting remains unfinished. None of those gaps
are hidden by the current Foundation status.
