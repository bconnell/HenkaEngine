# Documentation style

Henka documentation uses direct factual statements. Preserve negative wording
when it defines a real limitation, unsupported operation, invalid state, safety
condition, failure behavior, compatibility rule, or provenance boundary.

Remove rhetorical comparisons when the positive statement already communicates
the technical meaning. For example:

```text
Henka is planned to support 2D and 2.5D as first-class workflows, not as afterthoughts.
```

becomes:

```text
Henka is planned to support 2D and 2.5D as first-class workflows.
```

Review constructions such as `not as`, `rather than`, and `instead of`
manually. Keep a comparison when it communicates an actual contract, such as
the distinction between a supported path and an unavailable one, or rewrite it
as a direct statement that preserves that boundary.
