# Rigid-Body Physics

Henka Engine includes a scoped rigid-body physics v1 layer for small runtime scenes and engine testing.

## Supported Behavior

The public physics API provides:

- physics worlds with gravity and deterministic fixed-timestep stepping
- static bodies that collide without responding to force
- dynamic bodies driven by gravity, force, impulse, damping, and collision response
- kinematic bodies driven by assigned velocity rather than gravity or forces
- angular velocity and torque integration
- material restitution, static friction, dynamic friction, linear damping, and angular damping
- sphere, axis-aligned box, and plane colliders
- layer and mask filtering
- trigger overlap reporting without physical response
- collision and trigger enter, stay, and exit events
- raycasts against every supported collider shape
- optional links from physics bodies to real scene entities
- debug-shape and contact data for truthful runtime visualization
- transform validation that rejects non-finite and collapsed scale components
- physics allocations included in Henka's debug memory accounting
- selective contact and pair-history removal when a body is destroyed, with one EXIT event per active removed pair and unrelated queued events preserved

The broadphase currently iterates body pairs directly, which is appropriate for the small sandbox scene and deterministic tests.

Body IDs are monotonic within a world. Destroying a body cannot make its stale ID refer to a later body that reuses the same storage slot. Destruction reserves space for all required EXIT events before changing live state, then removes only contacts and current/previous pairs involving that body. Existing queued events remain available until the next simulation step, including events for unrelated pairs; the appended EXIT events appear once and are not repeated by the next step. Survivor `colliding` and `grounded` flags are recomputed only when the survivor had a contact with the destroyed body.

## Sandbox Physics QA

The sandbox panels open automatically on startup and reset-style launches, and `Physics QA` is reachable from the main Controls area. Starts have no selected physics body until you select one. Simulation remains opt-in until you use `Enable` or `Reset Demo`.

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

The demo links existing generic sample objects to bodies: the ground is a plane, the cubes use AABB colliders, the marker uses a sphere collider, one sample is a static obstacle, and one sample is a trigger volume. Collider debug lines come from the same collider data the solver tests, are clipped to the Scene View, and are not selectable scene objects. The visible ground uses a finite floor surface and grid; selecting it shows one bounded floor indicator rather than infinite plane bounds.

Physics simulation writes linked-body transforms to the real scene entities. Editor-style transforms continue to use the Action API and synchronize their linked body so gizmos and Transform QA remain usable.

## Current Limits

- Box collision is axis-aligned; rotated boxes are not oriented colliders.
- There are no mesh or concave colliders.
- There are no constraints, character controllers, vehicles, cloth, soft bodies, fluids, or ragdolls.
- Continuous collision detection is not implemented; the demo and tests use normal fixed-step conditions.
- Physics state is runtime state, not scene-authoring or save-data support.

Manual desktop QA remains necessary for judging collision feel, debug overlay clarity, and how physics interaction feels beside viewport tools.
