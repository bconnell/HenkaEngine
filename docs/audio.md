# Audio Runtime

Henka's Audio subsystem is a renderer-independent, headless-safe runtime
service in `engine/include/henka/audio.h`.

## Available

- Henka accepts PCM WAV, Ogg Vorbis (`.ogg` and `.oga`), MP3, and FLAC files
  through one bounded private decoder boundary. The production mixer receives
  validated mono or stereo float PCM with a sample rate between 8 kHz and
  192 kHz. Unsupported extensions, malformed containers, invalid metadata,
  non-finite decoded samples, and sources above the configured file or
  resident-memory limits fail with an asset-source error.

- Resident clips are loaded through the same confined project-relative path
  boundary used by other file-backed runtime data.
- A bounded stream API is available for long-form content in every supported
  format. It validates source metadata without decoding the full payload,
  keeps decoder state file-backed, reads caller-owned float frames in bounded
  windows, and can drive generation-checked voices or object-attached emitters
  without loading the entire source into resident sample memory. Stream reads
  are single-owner and caller-synchronized, matching the current deterministic
  mixer contract.
- The asset manager caches resident clips and metadata-first streams by
  canonical path. A path may expose both borrowed payload forms through one
  manager-owned asset identity; resident clips and streamed payloads can be
  reloaded transactionally in place while retaining bounded ownership.
  Malformed or unreadable replacements leave the prior payload and metadata
  live.
- A fixed-capacity voice pool uses generation-checked voice IDs. Voices bind to
  borrowed `henka_scene` and `henka_entity` objects and read the live entity
  transform while mixing.
- The bounded bus layout is Master, Music, SFX, Dialogue, Ambience, and UI.
  The current mixer supports per-bus gains, listener orientation, distance
  attenuation, stereo panning, looping, pitch, and deterministic interleaved
  stereo float-PCM output. Resident voices also support bounded pause, resume,
  restart, seek, gain, pitch, looping, spatial, and bus controls with
  generation-checked IDs.
- Destroyed or stale scene entities are rejected before they contribute audio.
  Scene and clip owners must stop dependent voices before destroying those
  borrowed objects.
- Scene Document v6 stores a bounded, value-only emitter configuration on the
  real authored object record and a value-only listener, including the
  resident/streamed storage choice. v1 through v3 documents load with the
  listener at its default, while v4 loads its authored listener and defaults
  storage to resident; loading a legacy document does not rewrite it.

The headless-safe mixer is the deterministic production-output boundary for
the renderer-independent portion of this slice. A client-only, caller-pumped
SDL3 output owner now opens a playback stream, accepts bounded stereo
float-PCM frames, and reports device/queue diagnostics. SDL3 audio hotplug and
format events are observed through an event watch that publishes atomic state;
the caller performs transactional recovery on its owner thread. Automatic
recovery is bounded to three attempts per event epoch, while explicit recovery
remains available. Successful recovery clears only queued device data. The
output owner does not create a background mixer thread; the caller owns scene
and Audio synchronization.

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
advancing their source positions. A packaged `--audio-smoke-test` loads
repository-owned WAV, Ogg Vorbis, MP3, and FLAC fixtures through the real asset
manager in both resident and metadata-first streamed modes. Each mode attaches
each payload to a real scene entity, mixes it through a live emitter, and
reaches the SDL output boundary. The focused decoder tests additionally cover
malformed inputs and direct decoder contracts.
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

The shared Script Host exposes typed `Audio.Play(entity)`,
`Audio.Stop(entity)`, `Audio.Restart(entity)`, `Audio.Pause(entity)`,
`Audio.Resume(entity)`, `Audio.IsPlaying(entity)`, `Audio.SetGain(entity, gain)`,
`Audio.SetPitch(entity, pitch)`, `Audio.SetLooping(entity, looping)`,
`Audio.SetSpatial(entity, spatial)`, `Audio.SetBus(entity, bus)`, and
`Audio.Seek(entity, frame)` bindings to both Lua and HenkaScript in Play. They
operate on the persisted object-to-emitter mapping; missing or stale emitter
bindings fail closed, while `IsPlaying` reports false.

Compressed Ogg Vorbis, MP3, and FLAC streams also support the same transactional
in-place reload contract as WAV streams. Replacement metadata and decoder state
are validated before adoption; a malformed replacement leaves the existing
stream and its active voices unchanged.

## Future work

Mixer effects, user-facing device selection/notification, expanded packaged
long-form content coverage, and broader spatial/occlusion features remain
future work. The supported decoder boundary is intentionally limited to
mono/stereo PCM WAV, Ogg Vorbis, MP3, and FLAC; additional codecs and
multichannel routing are outside the current Audio scope.
