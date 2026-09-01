# Rigid-Body Physics

Henka Engine includes a scoped rigid-body physics v1 layer for small runtime scenes and engine testing.

## Supported Behavior

The public physics API provides:

- physics worlds with gravity and deterministic fixed-timestep stepping
- static bodies that collide without responding to force
- dynamic bodies driven by gravity, force, impulse, damping, and collision response
- kinematic bodies driven by assigned velocity; gravity and forces do not drive them
- angular velocity and torque integration
- material restitution, static friction, dynamic friction, linear damping, and angular damping
- sphere, upright capsule, axis-aligned box, plane, and bounded static
  heightfield colliders
- layer and mask filtering
- trigger overlap reporting without physical response
- collision and trigger enter, stay, and exit events
- raycasts against every supported collider shape, including bounded heightfield traversal
- optional links from physics bodies to real scene entities
- a bounded public character-controller foundation backed by a real dynamic
  upright capsule body, with planar velocity limits, grounded jump queuing, and
  configurable slope-aware grounding, ground-normal reporting, explicit
  teleport/repositioning, contact-aware planar sliding against blocking
  contacts, and prepare/synchronize integration around the shared fixed-step
  world
- debug-shape and contact data for truthful runtime visualization
- transform validation that rejects non-finite and collapsed scale components
- physics allocations included in Henka's debug memory accounting
- atomic fixed substeps whose allocation failures retain the prior valid world
- selective contact and pair-history removal when a body is destroyed, with one EXIT event per active removed pair and unrelated queued events preserved

Heightfields are created with `henka_physics_collider_heightfield`. The source
array is borrowed only for the create or replacement call; the body copies and
owns the signed millimeter samples. Version 1 accepts static, identity-oriented
heightfields with at least a 2 by 2 grid and a bounded 4096 by 4096 dimension.
Sphere and axis-aligned box contacts use deterministic bounded corner/support
queries, derive terrain normals from neighboring samples, and honor the normal
layer/mask filters. Raycasts use a bounded cell-sized march and fail closed when
the requested range cannot be covered by the traversal budget. Replacement
copies the candidate before releasing the prior field, so invalid input or
allocation failure preserves the last valid collision representation.

Capsules are created with `henka_physics_collider_capsule`. The supported v1
capsule is upright on the world Y axis, uses a radius and cylindrical
half-height, and supports sphere/capsule, capsule/box, capsule/plane, and
capsule/heightfield contacts plus raycasts. An upright rotation around the Y
axis is accepted; tilted capsule transforms are rejected rather than treated
as a different shape. Nonuniform horizontal scale uses the larger X/Z scale
for a conservative bounded radius. Scene Document physics authoring still
supports its existing sphere and box shapes; runtime capsule authoring is not
implicitly serialized by this slice.

The broadphase currently iterates body pairs directly, which is appropriate for the small sandbox scene and deterministic tests.

Body IDs are monotonic within a world. Destroying a body cannot make its stale ID refer to a later body that reuses the same storage slot. Destruction reserves space for all required EXIT events before changing live state, then removes only contacts and current/previous pairs involving that body. Existing queued events remain available until the next simulation step, including events for unrelated pairs; the appended EXIT events appear once and are not repeated by the next step. Survivor `colliding` and `grounded` flags are recomputed only when the survivor had a contact with the destroyed body.

Each fixed substep builds a complete candidate using scratch body, contact, pair, and event arrays. The live world is replaced only after integration, collision response, and event classification all succeed. An allocation failure returns `HENKA_ERROR_OUT_OF_MEMORY`; a finite calculation that cannot produce representable, contract-valid body, collider, contact, or response state returns `HENKA_ERROR_NUMERIC_RANGE`. Either failure releases candidate storage, preserves the prior live arrays and accumulator, and performs no linked-scene writes. Callers may correct the input state and retry. The numeric boundary follows representable engine state and collider validity. It does not use an arbitrary gameplay-scale limit. In a catch-up update, each successful substep commits independently; if a later substep fails, the earlier results remain committed and the remaining accumulated time can be retried. Linked entity transforms are synchronized best-effort after a successful physics commit, so an invalid or removed link cannot corrupt the physics world.

## Sandbox Physics QA

The sandbox panels open automatically on startup and reset-style launches, and `Physics QA` is reachable from the Tools area. Starts have no selected physics body until you select one. Simulation remains opt-in until you use `Enable` or `Reset Demo`.

The QA view provides real controls for:

- enabling the demo
- pausing, resuming, and advancing one fixed step
- resetting the demo bodies to their test starting positions
- toggling gravity
- toggling collider and contact overlays
- changing the selected linked body's static, dynamic, or kinematic type
- making a supported selected body Dynamic and running gravity for quick drop tests
- applying upward or camera-forward impulses to a selected dynamic body
- clearing selected-body velocity
- raycasting from the camera

Body-type behavior is intentionally explicit in the UI:

- Static bodies do not move from gravity, forces, or impulses.
- Dynamic bodies fall and respond to gravity, forces, impulses, contacts, friction, restitution, and damping.
- Kinematic bodies do not fall from gravity and move only through explicit tool or code movement.

The demo links existing generic sample objects to bodies: the ground is a plane, the cubes use AABB colliders, the marker uses a sphere collider, one sample is a static obstacle, and one sample is a trigger volume. Collider debug lines come from the same collider data the solver tests, are clipped to the Scene View, and are not selectable scene objects. The visible ground uses a finite floor surface and grid; selecting it shows one bounded floor indicator. Infinite plane bounds are not shown.

Physics simulation writes linked-body transforms to the real scene entities. Editor-style transforms continue to use the Action API and synchronize their linked body so gizmos and Transform QA remain usable.

## Current Limits

- Box collision is axis-aligned; rotated boxes are not oriented colliders.
- Integration validates acceleration, damping, velocity, position, angular delta, and quaternion state before commit. Collision geometry, contact normals, penetration, contact points, impulses, friction, and positional correction are likewise required to remain finite and representable.
- There are no arbitrary mesh or concave colliders; heightfields are the only
  supported terrain-shaped collider.
- The character-controller foundation provides an upright capsule body, but it
  does not yet provide swept movement, slope traversal or response,
  moving-platform support, step offsets, vehicles, cloth, soft bodies, fluids,
  or ragdolls. It provides contact-aware planar sliding against blocking
  contacts, configurable slope-aware grounding classification, and the
  accepted ground normal.
- Continuous collision detection is not implemented; the demo and tests use normal fixed-step conditions.
- Physics state is runtime state, not scene-authoring or save-data support.

Manual desktop QA remains necessary for judging collision feel, debug overlay clarity, and how physics interaction feels beside viewport tools.
