# Audio Foundation

Henka's first Audio slice is a renderer-independent, headless-safe runtime
foundation in `engine/include/henka/audio.h`.

## Available foundation

- Resident PCM WAV clips are loaded through the same confined project-relative
  path boundary used by other file-backed runtime data.
- A fixed-capacity voice pool uses generation-checked voice IDs. Voices bind to
  borrowed `henka_scene` and `henka_entity` objects and read the live entity
  transform while mixing.
- The bounded bus layout is Master, Music, SFX, Dialogue, Ambience, and UI.
  The current mixer supports per-bus gains, listener orientation, distance
  attenuation, stereo panning, looping, pitch, and deterministic interleaved
  stereo float-PCM output.
- Destroyed or stale scene entities are rejected before they contribute audio.
  Scene and clip owners must stop dependent voices before destroying those
  borrowed objects.
- Scene Document v3 stores a bounded, value-only emitter configuration on the
  real authored object record, while v1 and v2 documents load with the Audio
  configuration defaulted off. Loading a legacy document does not rewrite it.

The headless-safe mixer is the deterministic production-output boundary for
this slice. It does not open an audio device, and it does not pretend that
generated test samples establish device or content-pipeline coverage.

## Current development

The next Audio slices must connect the core to the existing SDL3 platform
boundary, the manager-owned asset metadata path, Scene Document listener
configuration and runtime emitter instantiation, and the Sandbox Play
lifecycle. Those integrations must preserve the renderer-free dedicated-server
path and add real imported or authored-object coverage rather than test-only
entities.

## Future work

Streaming long-form assets, broader decoder coverage, mixer effects, hot reload,
editor controls, scripting bindings, package/runtime proof, device-loss
recovery, and broader spatial/occlusion features remain future work. None of
those gaps are hidden by the current Foundation status.
