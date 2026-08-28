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

Do not split sentences into artificial one-line fragments merely for appearance.
Readable documentation should still feel like documentation, not a slide deck.

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
materially clearer visually than through several paragraphs.

Suitable subjects include:

- scene and entity ownership;
- asset loading and dependency flow;
- Audio runtime flow;
- Play-session lifecycle;
- renderer pipeline stages;
- hierarchy and prefab relationships;
- roadmap dependencies.

Keep diagrams bounded and readable. A diagram should explain a relationship, not
repeat every implementation detail in a document.

## Code, commands, and paths

Use fenced code blocks for commands, code examples, configuration, serialized
formats, and multi-line paths. Use inline code for short identifiers, API names,
filenames, and single commands.

Examples should match supported product behavior. Do not publish illustrative
syntax that looks executable unless it is valid for the documented boundary.

## Collapsible detail

Use `<details>` blocks sparingly for long implementation notes, diagnostic
examples, or advanced material that most readers do not need immediately.

Do not hide critical limitations, safety requirements, failure behavior, or
setup steps inside collapsed sections.

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

## Current, planned, and historical material

Keep current behavior visually separate from future direction. Prefer explicit
headings such as **Available**, **In Progress**, and **Planned** rather than mixing
all three states in one paragraph.

Historical implementation notes should remain only when they help explain a
current contract, migration, compatibility rule, or important design decision.
Otherwise, prefer source history and release records over accumulating narrative
history in current user documentation.

## Links and navigation

Link to the authoritative detailed document instead of duplicating large sections
across several pages. Keep link labels descriptive.

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
- Markdown formatting regressions.

The governing principle is simple: preserve product truth, improve information
design.