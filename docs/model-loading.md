# Model Loading

Henka Engine currently supports a small, early OBJ loading path aimed at proving content-driven rendering in the sandbox.

## What works now

- OBJ files loaded from local assets
- vertex positions
- texture coordinates when present
- normals when present
- computed face normals when normals are missing
- triangle faces
- simple quad triangulation
- cached OBJ mesh loading through the asset manager

## What does not work yet

- MTL material import
- model hierarchies
- skeletal animation
- glTF
- editor import workflows
- model reloading

## Failure behavior

If an OBJ file is missing or cannot be parsed:

- the engine logs a clear error
- the sandbox keeps running
- the asset manager returns a fallback mesh so the scene stays visible

The sandbox includes both a valid sample OBJ asset and a missing-model example so this behavior is easy to inspect during manual QA.

## Sample asset

`assets/models/henka_marker.obj` is a small self-authored sample asset included for testing and demonstration. It is intentionally simple and readable.
