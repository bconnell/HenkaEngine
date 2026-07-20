# Repository Integrity

Henka's public repository checks separate wording policy from technical integrity.

`check_public_repo_hygiene.ps1` checks first-party public text for the project's limited wording rules.

`check_repository_integrity.ps1` checks:

- tracked build output, logs, caches, binaries, local environment files, and signing material
- common private-key and access-token signatures
- merge-conflict markers
- PowerShell parser errors and known Windows PowerShell 5.1 incompatibilities
- process launches that bypass the shared native-process contract
- external GitHub Actions that are not pinned to a full commit
- the pinned SDL source revision
- required private-key ignore patterns

The integrity check runs before compilation in Windows CI. It can also be run locally:

```powershell
.\scripts\check_repository_integrity.ps1
```

Generated output remains under ignored `build/` and `out/` paths. The check does not rewrite, clean, reset, or remove repository content.
