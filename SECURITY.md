# Security Policy

Henka Engine is an early-stage open source C engine project. Security reports are taken seriously, especially when they affect build scripts, packaged output, file loading, asset parsing, local paths, or project templates.

## Supported versions

The `main` branch is the active development line. Security fixes should target `main` unless a release branch is created later.

## Reporting a security issue

Please do not open a public issue for a suspected security vulnerability.

Use GitHub's private vulnerability reporting feature if it is available on this repository. If that is not available, contact the maintainer through the public GitHub profile so a private reporting path can be arranged.

When reporting an issue, include:

1. A clear description of the problem.
2. Steps to reproduce the issue.
3. The affected file, script, asset path, or workflow when known.
4. Any relevant platform details.
5. Whether the issue affects local development, packaged builds, or external project templates.

## Scope

Security-sensitive areas include:

1. Build and packaging scripts.
2. File loading and asset parsing.
3. Local settings and save-data paths.
4. External project templates.
5. Runtime diagnostics.
6. Public documentation that could cause unsafe usage.
7. Any future network, scripting, plugin, or editor workflow if those are added later.

## Current security expectations

Henka Engine currently has no accounts, telemetry, analytics, cloud sync, payment logic, or network-backed runtime behavior.

Do not include private files, credentials, API keys, local user data, generated settings, packaged output, screenshots with private information, or machine-specific logs in issues, pull requests, commits, or test fixtures.

## Response expectations

This project is maintained as time allows. Acknowledgement and fixes may depend on severity, reproducibility, and project scope. Reports with clear reproduction steps are easier to review and fix.
