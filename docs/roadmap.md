# Roadmap

Henka Engine is an early-stage open source C engine. The current goal is to build a stable runtime foundation first, then grow toward practical tools for 3D, 2D, and 2.5D projects.

This roadmap is a direction guide, not a release schedule. Priorities may change as the engine matures, testing finds issues, or core systems need more hardening.

## Current focus

The current work is focused on making the 3D sandbox and engine core dependable enough to build on.

Current priorities include:

1. Stable engine startup and shutdown.
2. Clear platform, renderer, input, scene, and camera boundaries.
3. Reliable object selection and transform behavior.
4. Local settings and save-data foundations.
5. Asset loading for shaders, textures, and simple model data.
6. A packaged sandbox that can be tested without private setup.
7. Documentation that stays aligned with what the engine actually does.
8. Test coverage for core behavior that should not depend on manual QA.

## Near-term priorities

The next development work is focused on making existing systems easier to test, easier to use, and harder to break.

1. Add and harden local action commands for validated scene and object operations.
2. Keep viewport interaction test helpers aligned with the real sandbox behavior.
3. Finish manual QA for transform gizmos and fix any remaining interaction problems.
4. Add Windows CMake CI for public build and test checks.
5. Improve sandbox usability, help text, and first-run guidance.
6. Keep object inspection, transform tools, and viewport behavior consistent.
7. Keep external game project templates working against the current engine.

## Workspace and tools

Henka is moving toward a practical developer workspace, but this should happen in layers.

Planned workspace improvements include:

1. Resizable docked panels.
2. Floating panels.
3. Saved and resettable workspace layouts.
4. Better status messages and tool feedback.
5. A clearer scene hierarchy.
6. Numeric transform editing.
7. Undo and redo for basic scene operations.
8. Scene save and load support.

These features should not be added as placeholder UI. Each one should be wired into the engine, tested, documented, and useful before it is treated as complete.

## Asset and material workflow

The asset pipeline is still early. The goal is to move from loose sample assets toward a clearer project workflow.

Planned asset and material work includes:

1. Stronger asset metadata.
2. Better model import coverage.
3. Texture and material assignment tools.
4. Clear missing-asset fallback behavior.
5. Material editing.
6. Shader selection.
7. Procedural shader planning and safe parameter handling.
8. External project asset roots.

Procedural shader work should come after the material system is stable enough to support it cleanly.

## 2D and 2.5D direction

Henka is planned to support 2D and 2.5D as first-class workflows, not as afterthoughts.

Planned 2D work includes:

1. A dedicated 2D renderer path.
2. Sprites.
3. Texture regions.
4. Layers.
5. A 2D camera.
6. A focused 2D sample.

Planned 2.5D work includes:

1. Side-scroller camera presets.
2. Top-down camera presets.
3. Isometric camera presets.
4. Layered depth.
5. Parallax.
6. Tools that make 2D-style layout in 3D space easier to manage.

## Longer-term systems

Longer-term work may include:

1. Physics.
2. Audio.
3. Scripting or extension support.
4. Additional renderer backends.
5. Release packaging.
6. Versioned builds.
7. Checksums and release verification.
8. Provenance for release artifacts.

These systems will require careful design because they affect safety, project structure, and long-term maintenance.

## External project workflow

Henka Engine should remain a reusable engine repository. Real games built with Henka should live in separate repositories.

Planned external project work includes:

1. Stronger starter templates.
2. Clear project configuration.
3. Separate asset and scene roots.
4. Build guidance for external projects.
5. Packaging guidance for external projects.
6. Validation scripts that prove templates still work.

## Sponsorship supported work

Henka Engine is open source, and sponsorship helps support the time needed to continue development.

Funding can help with engine development, sandbox usability, documentation, examples, packaged builds, testing, asset workflow improvements, and future workspace tools.

Sponsorship does not change the license, purchase feature priority, or override the project roadmap. Roadmap decisions remain based on stability, maintainability, scope, and usefulness to the engine.

## Current limitations

Henka is still early. Some systems are intentionally limited while the engine foundation is being built.

Current limitations include:

1. The sandbox is an engine sample and QA target, not a game.
2. The transform gizmo workflow still needs manual desktop QA for visual feel and mouse comfort.
3. Scene saving and loading are not complete authoring workflows yet.
4. The UI is useful for inspection and testing, but it is not a full editor.
5. Asset loading is still limited.
6. 2D and 2.5D workflows are planned, but not implemented yet.
7. Physics, audio, scripting, and release distribution are later milestones.
