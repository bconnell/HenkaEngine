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

A leftover backup is never deleted automatically. Packaging stops and reports its location so it can be inspected before another refresh.

A package created from a working tree is identified as `working-tree`. A package created after commit and a clean rebuild is identified as `clean`.
