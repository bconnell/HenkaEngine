# Documentation Presentation Standard

Henka documentation should be technically complete, truthful, and easy to scan.
Presentation improvements must never hide limitations, inflate capability claims,
or remove important engineering detail merely to make a page shorter.

## Goals

Documentation should help a reader quickly answer four questions:

1. What does this subsystem or workflow do?
2. What works today?
3. What remains incomplete or planned?
4. Where can the reader find implementation, validation, or usage details?

## Page structure

Long documents should begin with a short summary and, when useful, a local table
of contents. Prefer clear sections over long uninterrupted prose.

Use headings often enough that a reader can navigate by scanning. Good recurring
section names include:

- Summary
- Status
- Architecture
- Available
- In Progress
- Planned
- Limitations
- Validation
- Usage
- Failure behavior
- Related documentation

Not every document needs every section. Use the smallest structure that makes the
material easier to understand.

## Paragraphs

Avoid screen-length walls of text. If a paragraph contains multiple distinct
requirements, capabilities, lifecycle stages, or limitations, split it into
smaller paragraphs or another structure.

Prefer prose for explanation and reasoning. Prefer lists or tables for repeated
facts, requirements, states, comparisons, or inventories.

Keep sentence and paragraph structure natural. Documentation should remain
readable at normal desktop and narrow-window widths.

## Direct declarative wording

State facts, requirements, limitations, and plans directly.

Avoid rhetorical comparison constructions that sound conversational, defensive,
or machine-generated. Common patterns to remove include:

- `rather than ...`
- `instead of ...`
- `not X, but Y`
- `this is not X; it is Y` when a direct statement can carry the same meaning
- repeated contrast framing used only to emphasize a point

Prefer a single declarative sentence whenever the intended meaning is clear.

Examples:

- Write `The runtime uses the production scene graph.`
- Write `Linux support is planned.`
- Write `This subsystem remains a Foundation.`
- Write `Automated visual evidence does not satisfy human visual QA.`

Use contrast only when the distinction is technically necessary to prevent a
real misunderstanding, define a contract boundary, or document incompatible
behavior.

## Lists and tables

Use lists when order, steps, requirements, supported operations, or known gaps
matter.

Use tables when readers need to compare several items across the same attributes.
Examples include capability status, platform support, file formats, lifecycle
states, validation gates, or feature maturity.

Avoid very wide tables when normal prose or a list would be easier to read on a
narrow display.

## Status callouts

Short blockquotes may be used for important status information, for example:

> **Status:** Foundation  
> **Current focus:** Runtime integration and end-user workflow completion

Status wording must remain consistent with the repository's authoritative
capability definitions. Presentation does not change capability truth.

## Diagrams

Use Mermaid when architecture, ownership, lifecycle, dependency, or data flow is
materially clearer in visual form.

Suitable subjects include:

- scene and entity ownership;
- asset loading and dependency flow;
- Audio runtime flow;
- Play-session lifecycle;
- renderer pipeline stages;
- hierarchy and prefab relationships;
- roadmap dependencies.

Keep diagrams bounded and readable. A diagram should explain a relationship and
stay focused on the details needed for that relationship.

## Code, commands, and paths

Use fenced code blocks for commands, code examples, configuration, serialized
formats, and multi-line paths. Use inline code for short identifiers, API names,
filenames, and single commands.

Examples should match supported product behavior. Do not publish illustrative
syntax that looks executable unless it is valid for the documented boundary.

## Collapsible detail

Use `<details>` blocks sparingly for long implementation notes, diagnostic
examples, or advanced material that most readers do not need immediately.

Keep critical limitations, safety requirements, failure behavior, and setup
steps visible in the normal document flow.

## Capability truth

Documentation presentation work must preserve:

- current capability status;
- explicit limitations;
- production-versus-foundation distinctions;
- ownership and lifetime requirements;
- failure behavior;
- platform scope;
- persistence and compatibility boundaries;
- validation and evidence requirements.

A documentation-only cleanup must not upgrade a capability or imply that planned
work exists.

## Live documentation maintenance

Documentation is part of the active product surface. Keep it current as Henka
changes.

Update the relevant documentation in the same coherent change whenever an
implementation changes:

- public behavior or supported workflows;
- capability status or scope;
- commands, paths, configuration, or build steps;
- platform or renderer support;
- persistence or file-format behavior;
- ownership, lifecycle, or failure contracts;
- editor controls or user-visible terminology;
- validation, packaging, or external-project behavior;
- roadmap sequencing or a committed future direction.

Fix stale documentation when it is encountered and the correct state is clear
from the production code, tests, and current project direction. Keep those edits
scoped to the affected documentation.

README, roadmap, capability, subsystem, help, and platform documentation should
not remain knowingly stale at a publication boundary. A disagreement between
product behavior and live documentation is a documentation defect and should be
resolved from product truth.

The repository-wide presentation cleanup may proceed independently. New product
work must still keep the live documentation it touches current.

## Current, planned, and historical material

Keep current behavior visually separate from future direction. Prefer explicit
headings such as **Available**, **In Progress**, and **Planned** when they make the
state easier to scan.

Historical implementation notes should remain only when they help explain a
current contract, migration, compatibility rule, or important design decision.
Source history and release records should carry implementation history that no
longer helps explain the current product.

## Links and navigation

Link to the authoritative detailed document and keep duplicate overview text
small. Keep link labels descriptive.

Long overview documents should point readers toward focused subsystem pages.
Focused subsystem pages should link back to relevant architecture, build,
capability, and roadmap documentation where useful.

## Visual restraint

"Prettier" means easier to read, navigate, and understand.

Avoid decorative badge walls, excessive emoji, oversized banners, or repeated
visual elements that do not communicate useful information. The Henka lockup and
other branding should support navigation and identity without competing with the
technical content.

## Validation

When documentation changes alongside product changes, run the repository's
existing documentation truth, hygiene, integrity, and relevant product gates.

For documentation-only presentation work, review the diff specifically for:

- accidental capability changes;
- removed limitations;
- broken links or anchors;
- altered commands or paths;
- duplicated or contradictory status wording;
- Mermaid syntax errors;
- Markdown formatting regressions;
- unnecessary rhetorical comparison wording.

The governing principle is simple: preserve product truth, improve information
design.
