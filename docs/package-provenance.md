# Package Provenance

Henka's Windows package flow records and verifies the build that produced the runnable sandbox.

## Build record

`build/henka-build-info.json` is written transactionally after a successful Windows build. It records:

- the full source commit
- whether tracked or untracked working-tree changes were present
- the local branch when one is checked out
- the GitHub ref and pull-request head ref when available
- whether the checkout is detached
- build configuration and architecture
- CMake path and version
- the selected executable path
- the executable SHA-256
- build and file timestamps

Detached checkouts are valid. This is required for pull-request validation, where the checked-out commit may not have a local branch name.

The file stays under the ignored build tree and is not committed.

## Package checks

The package command requires an explicit validated build configuration. It rejects:

- a missing or obsolete build record
- a build record from another commit or source state
- an executable from another configuration
- an executable whose SHA-256 no longer matches the build record
- a copied executable whose SHA-256 differs from the source build
- package inputs that contain reparse points

`PACKAGE_INFO.txt` carries the verified commit, ref, source state, configuration, architecture, and executable hashes into the runnable folder.

## Transactional refresh

A package is assembled in a unique staging directory before it replaces the active package. Existing user data is copied into the staged package unless `-ResetUserData` is used.

The previous package remains untouched until the staged package is complete. During activation, the previous package is moved to a unique backup, the staged package becomes active, and the backup is removed only after activation succeeds. A failure before activation restores the previous package.

A leftover staging directory still stops packaging for inspection because it may represent an incomplete transaction. A leftover backup is removed automatically only when the active package is independently proven complete and both the active package and backup are free of reparse points. If that proof or cleanup fails, packaging stops with the exact retained path instead of reporting success.

A package created from a working tree is identified as `working-tree`. A package created after commit and a clean rebuild is identified as `clean`.

## KTX2/Basis provenance

The KTX2/Basis path uses the pinned KhronosGroup KTX-Software revision
`91ace88675ac59a97e55d0378a6602a9ae6b98bd` (the v4.3.2 release source). Henka
builds its static library with tools, tests, Vulkan upload, and OpenGL upload
disabled. KTX1 compatibility objects remain present because libktx's public
stream vtable references them, but Henka accepts only KTX2 at its boundary.
Only the internal C boundary in
`engine/src/renderer/ktx_boundary.c` includes `ktx.h`; the public engine API
does not expose KTX types. Downstream packages must preserve KTX-Software's
`LICENSE.md`, `LICENSES/`, and `NOTICE.md` notices when redistributing it.
