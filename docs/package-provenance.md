# Package Provenance

Henka's Windows package flow records the build that produced the runnable sandbox.

## Build record

`build/henka-build-info.json` is created after a successful Windows build. It records:

- the full source commit
- whether tracked or untracked working-tree changes were present
- branch and build configuration
- architecture and CMake version
- the selected executable path
- the executable SHA-256
- build and file timestamps

The file stays under the ignored build tree and is not committed.

## Package checks

The package command requires an explicit validated build configuration. It rejects:

- a missing build record
- a build record from another commit or source state
- an executable from another configuration
- an executable whose SHA-256 no longer matches the build record
- a copied executable whose SHA-256 differs from the source build

`PACKAGE_INFO.txt` carries the verified commit, source state, configuration, architecture, and executable hashes into the runnable folder.

A package created from a working tree is identified as `working-tree`. A package created after commit and a clean rebuild is identified as `clean`.
