# Action API

Henka Engine now includes a small local Action API for validated scene and object operations.

The goal is straightforward:

- the engine owns authority
- tools and tests request actions
- Henka validates the request
- the action either executes or returns a structured failure

This is a local engine foundation. It is not a network service, cloud bridge, scripting runtime, plugin system, or arbitrary code execution path.

## What Action API v1 covers

The current action context supports:

- scene summary queries
- object listing
- adding a primitive-backed logical scene object
- deleting an object
- renaming an object
- selecting an object
- reading the selected object
- reading object details
- setting position
- setting rotation
- setting scale
- moving by delta
- rotating by delta
- scaling by multiplier
- resetting a transform when a default transform is registered
- hiding and showing objects
- focusing a camera on an object when a camera context is attached

The current primitive create path is intentionally lightweight. It creates a valid scene object with a name, tag, transform, visibility state, and local bounds. It does not assign meshes or materials automatically.

## Structured results

Every action returns a `henka_action_result` with practical state for tools and tests:

- success or failure
- command name
- action status
- underlying engine result
- affected entity
- selected entity
- before transform when relevant
- after transform when relevant
- scene summary when relevant
- object details when relevant
- short status message

This keeps tests and future tool surfaces from guessing whether a request really changed the scene.

## Dry-run validation

`henka_action_validate` runs the same validation path as execution but does not mutate scene state.

This is useful for:

- testing expected failures
- checking transforms before applying them
- making future workspace tools safer
- proving that a command would succeed without changing the scene yet

Dry-run does not create objects, delete objects, move objects, or update selection.

## Selection and helper safety

Action API v1 rejects helper entities as normal user-object targets.

That means internal gizmo helper pieces are not valid targets for:

- selection
- rename
- transform mutation
- visibility actions
- camera focus

This is one of the guardrails that keeps scene tools manipulating the real selected object instead of an internal helper.

## Current limitations

- Action API v1 uses current entity handles, not long-term stable project object IDs.
- Reset transform only works when a default transform has been registered for that entity.
- Primitive creation is a logical scene-object foundation, not a full asset-instancing workflow.
- This is not scene saving.
- This is not undo or redo.
- This does not add scripting, plugins, networking, or assistant runtime control.

## Viewport interaction testing

The viewport interaction test helpers now work alongside the Action API:

- viewport coordinate conversion
- world-to-screen projection
- projected gizmo handle models
- screen-space gizmo hit testing
- deterministic gizmo drag math

Together, those foundations let tests prove outcomes such as:

- the selected object moved
- the selected object rotated
- the selected object scaled
- a near-gizmo click did not accidentally select another object

Manual QA is still needed for visual feel, handle readability, and mouse comfort.
